/*
 * SpiderKit PS5 Trophy Unlocker launcher
 *
 * Copyright (C) 2026 zer0day
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>

#include <ps5/klog.h>

#include "elfldr.h"
#include "notify.h"
#include "pt.h"

#define CONFIG_PATH "/data/trophy_unlocker_config.txt"
#define ENGINE_MIN_SIZE 4096u
#define ENGINE_MAX_SIZE (4u * 1024u * 1024u)

typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
  uint32_t app_type;
  char title_id[10];
  char unknown2[0x3c];
} app_info_t;

typedef struct game_target {
  pid_t pid;
  char title_id[10];
} game_target_t;

int sceKernelGetAppInfo(pid_t pid, app_info_t *info);

extern unsigned char embedded_engine_start[]
  __asm__("_binary_engine_trophy_unlocker_engine_elf_start");
extern unsigned char embedded_engine_end[]
  __asm__("_binary_engine_trophy_unlocker_engine_elf_end");

static size_t
embedded_engine_size(void) {
  return (size_t)(embedded_engine_end - embedded_engine_start);
}

static int
is_ascii_digit(char c) {
  return c >= '0' && c <= '9';
}

static int
is_supported_game_title(const char *title_id) {
  if(strncmp(title_id, "PPSA", 4) &&
     strncmp(title_id, "PPSB", 4) &&
     strncmp(title_id, "CUSA", 4)) {
    return 0;
  }

  for(size_t i = 4; i < 9; i++) {
    if(!is_ascii_digit(title_id[i])) {
      return 0;
    }
  }

  return title_id[9] == '\0';
}

static int
read_title_id(pid_t pid, char title_id[10]) {
  app_info_t info;

  memset(&info, 0, sizeof(info));
  memset(title_id, 0, 10);

  if(sceKernelGetAppInfo(pid, &info)) {
    return -1;
  }

  memcpy(title_id, info.title_id, 9);
  title_id[9] = '\0';
  return is_supported_game_title(title_id) ? 0 : -1;
}

/*
 * Return values:
 *  -1: process query failed
 *   0: no supported game
 *   1: exactly one supported game
 *   2: ambiguous (more than one supported game)
 */
static int
find_running_game(game_target_t *target) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  uint8_t *buf = NULL;
  int matches = 0;

  memset(target, 0, sizeof(*target));

  if(sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size) {
    return -1;
  }

  buf = malloc(buf_size);
  if(!buf) {
    return -1;
  }

  if(sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
    free(buf);
    return -1;
  }

  for(uint8_t *ptr = buf; ptr < buf + buf_size;) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    char title_id[10];

    if(ki->ki_structsize <= 0 || ptr + ki->ki_structsize > buf + buf_size) {
      matches = -1;
      break;
    }
    ptr += ki->ki_structsize;

    if(ki->ki_pid == getpid()) {
      continue;
    }
    if(strcmp(ki->ki_comm, "eboot.bin") &&
       strcmp(ki->ki_tdname, "eboot.bin")) {
      continue;
    }
    if(read_title_id(ki->ki_pid, title_id)) {
      continue;
    }

    matches++;
    if(matches == 1) {
      target->pid = ki->ki_pid;
      memcpy(target->title_id, title_id, sizeof(target->title_id));
    } else {
      break;
    }
  }

  free(buf);
  return matches;
}

static int
validate_embedded_engine(void) {
  const Elf64_Ehdr *ehdr;
  size_t size = embedded_engine_size();

  if(size < ENGINE_MIN_SIZE || size > ENGINE_MAX_SIZE ||
     size < sizeof(Elf64_Ehdr)) {
    return -1;
  }

  ehdr = (const Elf64_Ehdr *)embedded_engine_start;
  if(memcmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
     ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
     ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
     ehdr->e_machine != EM_X86_64 ||
     (ehdr->e_type != ET_DYN && ehdr->e_type != ET_EXEC)) {
    return -1;
  }

  if(ehdr->e_phentsize != sizeof(Elf64_Phdr) || !ehdr->e_phnum ||
     ehdr->e_phoff > size ||
     (size_t)ehdr->e_phnum > (size - ehdr->e_phoff) / sizeof(Elf64_Phdr)) {
    return -1;
  }

  return elfldr_sanity_check(embedded_engine_start, size);
}

static int
write_all_selection(void) {
  static const char selection[] = "mode=all\n";
  size_t done = 0;
  int sync_error;
  int close_error;
  int fd = open(CONFIG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  if(fd < 0) {
    return -1;
  }

  while(done < sizeof(selection) - 1) {
    ssize_t written = write(fd, selection + done,
                            sizeof(selection) - 1 - done);
    if(written <= 0) {
      close(fd);
      unlink(CONFIG_PATH);
      return -1;
    }
    done += (size_t)written;
  }

  sync_error = fsync(fd);
  close_error = close(fd);
  if(sync_error || close_error) {
    unlink(CONFIG_PATH);
    return -1;
  }
  return 0;
}

static int
target_still_matches(const game_target_t *target) {
  char current_title[10];

  if(read_title_id(target->pid, current_title)) {
    return 0;
  }
  return !strcmp(target->title_id, current_title);
}

int
main(void) {
  game_target_t target;
  int matches;

  klog_puts("[PS5 TROPHY UNLOCKER] launcher v1.0.0 starting");

  if(validate_embedded_engine()) {
    notify("PS5 TROPHY UNLOCKER\nABORTED: embedded ELF validation failed");
    klog_puts("[PS5 TROPHY UNLOCKER] invalid embedded engine");
    return 10;
  }

  matches = find_running_game(&target);
  if(matches < 0) {
    notify("PS5 TROPHY UNLOCKER\nABORTED: process scan failed");
    return 11;
  }
  if(matches == 0) {
    notify("PS5 TROPHY UNLOCKER\nNO GAME FOUND\nLaunch one PPSA/PPSB/CUSA game, then retry");
    return 12;
  }
  if(matches > 1) {
    notify("PS5 TROPHY UNLOCKER\nABORTED: multiple game processes found");
    return 13;
  }

  if(write_all_selection()) {
    notify("PS5 TROPHY UNLOCKER\nABORTED: cannot prepare mode=all");
    return 14;
  }

  notify("PS5 TROPHY UNLOCKER\nTARGET: %s (PID %d)\nUnlocking this game only...",
         target.title_id, target.pid);

  if(pt_attach(target.pid)) {
    unlink(CONFIG_PATH);
    notify("PS5 TROPHY UNLOCKER\nABORTED: cannot attach to %s",
           target.title_id);
    return 15;
  }

  /* Freeze-and-recheck closes the PID reuse / title-switch race window. */
  if(!target_still_matches(&target)) {
    pt_detach(target.pid, 0);
    unlink(CONFIG_PATH);
    notify("PS5 TROPHY UNLOCKER\nABORTED: running title changed");
    return 16;
  }

  if(elfldr_exec(target.pid, -1, embedded_engine_start)) {
    unlink(CONFIG_PATH);
    notify("PS5 TROPHY UNLOCKER\nINJECTION FAILED: %s",
           target.title_id);
    return 17;
  }

  notify("PS5 TROPHY UNLOCKER\nENGINE STARTED IN %s\nWait for the trophy result",
         target.title_id);
  klog_printf("[PS5 TROPHY UNLOCKER] engine injected into %s pid=%d\n",
              target.title_id, target.pid);
  return 0;
}
