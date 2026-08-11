/*
 * SpiderKit PS5 Trophy Unlocker -- firmware 10.01 direct Trophy2 build
 *
 * Copyright (C) 2026 zer0day
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <ps5/kernel.h>
#include <ps5/klog.h>

#include "notify.h"
#include "pt.h"

#define APP_INFO_SIZE 0x60u
#define APP_TITLE_OFFSET 0x14u
#define APP_TITLE_LEGACY_OFFSET 0x10u
#define TITLE_TOKEN_SIZE 14u
#define PROCESS_NAME_SIZE 20u
#define PROCESS_PATH_SIZE 512u
#define PS5_KINFO_PID_OFFSET 72u
#define PS5_KINFO_TDNAME_OFFSET 394u
#define PS5_KINFO_COMM_OFFSET 447u

#define FW_1001_MIN 0x10010000u
#define FW_1001_MAX 0x1001ffffu

/* Firmware 10.01 Trophy2/ShellCore values adapted from Team PHU's
 * MIT-licensed PS5-PHU-Trophy-System research. See THIRD_PARTY_NOTICES.md. */
#define SHELLCORE_TITLE_ID "NPXS40082"
#define SHELLCORE_IMAGE_BASE 0x01000000ul
#define SHELLCORE_AUTH_VA 0x019e84e0ul
#define SHELLCORE_AUTH_DELTA (SHELLCORE_AUTH_VA - SHELLCORE_IMAGE_BASE)
#define SHELLCORE_PATCH_SIZE 6u

#define TROPHY2_CREATE_CONTEXT_OFFSET 0x150ul
#define TROPHY2_STATE_OFFSET 0x18080ul
#define TROPHY2_SLOT_FIRST 0x158ul
#define TROPHY2_SLOT_END 0x350ul
#define TROPHY2_SLOT_STEP 8ul
#define TROPHY2_NODE_CONTEXT_OFFSET 0x08ul
#define TROPHY2_NODE_ACTIVE_OFFSET 0x14ul
#define TROPHY2_MAX_IDS 128u

#define SCRATCH_SIZE 4096u
#define SCRATCH_GAME_DETAILS 0x000u
#define SCRATCH_GAME_DATA 0x100u
#define SCRATCH_HANDLE_ID 0x180u
#define SCRATCH_UNLOCK_SPEC 0x200u
#define GAME_DETAILS_SIZE 0x98u
#define GAME_DATA_SIZE 0x18u

typedef struct app_info_raw {
  uint8_t bytes[APP_INFO_SIZE];
} app_info_raw_t;

typedef struct game_target {
  pid_t pid;
  int title_known;
  char title_token[TITLE_TOKEN_SIZE];
  char process_name[PROCESS_NAME_SIZE];
} game_target_t;

typedef struct scan_stats {
  int processes;
  int eboot_candidates;
  int title_candidates;
} scan_stats_t;

typedef struct trophy_symbols {
  uint32_t trophy2_handle;
  intptr_t create_context;
  intptr_t create_handle;
  intptr_t destroy_handle;
  intptr_t get_game_info;
  intptr_t debug_unlock;
} trophy_symbols_t;

typedef struct trophy_game_info {
  uint32_t total;
  uint32_t unlocked;
} trophy_game_info_t;

int sceKernelGetAppInfo(pid_t pid, app_info_raw_t *info);

static const uint8_t shellcore_original[SHELLCORE_PATCH_SIZE] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x57
};

static const uint8_t shellcore_patch[SHELLCORE_PATCH_SIZE] = {
  0xb8, 0x01, 0x00, 0x00, 0x00, 0xc3
};

static int
is_ascii_digit(char c) {
  return c >= '0' && c <= '9';
}

static int
is_five_digit_token(const char *token) {
  for(size_t i = 0; i < 5; i++) {
    if(!is_ascii_digit(token[i])) {
      return 0;
    }
  }
  return token[5] == '\0';
}

static int
is_supported_full_title(const char *token) {
  if(strncmp(token, "PPSA", 4) &&
     strncmp(token, "PPSB", 4) &&
     strncmp(token, "CUSA", 4)) {
    return 0;
  }
  for(size_t i = 4; i < 9; i++) {
    if(!is_ascii_digit(token[i])) {
      return 0;
    }
  }
  return token[9] == '\0';
}

static void
ascii_upper_prefix(char *text) {
  for(size_t i = 0; i < 4 && text[i]; i++) {
    if(text[i] >= 'a' && text[i] <= 'z') {
      text[i] = (char)(text[i] - ('a' - 'A'));
    }
  }
}

static size_t
copy_ascii_field(char *dst, size_t dst_size, const uint8_t *src,
                 size_t src_size) {
  size_t length = 0;

  if(!dst_size) {
    return 0;
  }
  while(length + 1 < dst_size && length < src_size) {
    uint8_t value = src[length];
    if(!value || value < 0x20 || value > 0x7e) {
      break;
    }
    dst[length++] = (char)value;
  }
  while(length && dst[length - 1] == ' ') {
    length--;
  }
  dst[length] = '\0';
  return length;
}

static int
is_supported_title_token(const char *token) {
  return is_five_digit_token(token) || is_supported_full_title(token);
}

static int
read_app_info(pid_t pid, app_info_raw_t *info) {
  memset(info, 0, sizeof(*info));
  return sceKernelGetAppInfo(pid, info);
}

static int
read_title_token_at(const app_info_raw_t *info, size_t offset,
                    char token[TITLE_TOKEN_SIZE]) {
  memset(token, 0, TITLE_TOKEN_SIZE);
  copy_ascii_field(token, TITLE_TOKEN_SIZE, info->bytes + offset,
                   APP_INFO_SIZE - offset);
  ascii_upper_prefix(token);
  return is_supported_title_token(token) ? 0 : -1;
}

static int
read_title_token(pid_t pid, char token[TITLE_TOKEN_SIZE]) {
  app_info_raw_t info;

  memset(token, 0, TITLE_TOKEN_SIZE);
  if(read_app_info(pid, &info)) {
    return -1;
  }
  if(!read_title_token_at(&info, APP_TITLE_OFFSET, token)) {
    return 0;
  }
  return read_title_token_at(&info, APP_TITLE_LEGACY_OFFSET, token);
}

static int
app_info_has_title(pid_t pid, const char *wanted) {
  app_info_raw_t info;
  char token[TITLE_TOKEN_SIZE];
  const size_t offsets[] = {APP_TITLE_OFFSET, APP_TITLE_LEGACY_OFFSET};

  if(read_app_info(pid, &info)) {
    return 0;
  }
  for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    memset(token, 0, sizeof(token));
    copy_ascii_field(token, sizeof(token), info.bytes + offsets[i],
                     APP_INFO_SIZE - offsets[i]);
    ascii_upper_prefix(token);
    if(!strcmp(token, wanted)) {
      return 1;
    }
  }
  return 0;
}

static int
starts_with_ci(const char *text, const char *prefix) {
  if(!text || !prefix) {
    return 0;
  }
  while(*prefix) {
    char left = *text++;
    char right = *prefix++;
    if(left >= 'A' && left <= 'Z') {
      left = (char)(left + ('a' - 'A'));
    }
    if(right >= 'A' && right <= 'Z') {
      right = (char)(right + ('a' - 'A'));
    }
    if(left != right) {
      return 0;
    }
  }
  return 1;
}

static int
starts_with_eboot(const char *name, size_t available) {
  static const char expected[] = "eboot.bin";

  if(!name || available < sizeof(expected) - 1) {
    return 0;
  }
  return starts_with_ci(name, expected);
}

static int
read_process_path(pid_t pid, char path[PROCESS_PATH_SIZE]) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, pid};
  size_t size = PROCESS_PATH_SIZE;

  memset(path, 0, PROCESS_PATH_SIZE);
  if(sysctl(mib, 4, path, &size, NULL, 0) || !size) {
    return -1;
  }
  path[PROCESS_PATH_SIZE - 1] = '\0';
  return 0;
}

static const char *
path_basename(const char *path) {
  const char *base = path;

  if(!path) {
    return "";
  }
  for(const char *cursor = path; *cursor; cursor++) {
    if(*cursor == '/') {
      base = cursor + 1;
    }
  }
  return base;
}

static int
path_is_eboot(const char *path) {
  const char *base = path_basename(path);
  return starts_with_eboot(base,
                           PROCESS_PATH_SIZE - (size_t)(base - path));
}

static int
path_is_shellcore(const char *path) {
  const char *base = path_basename(path);
  return starts_with_ci(base, "SceShellCore.elf") ||
         starts_with_ci(base, "SceShellCore");
}

static void
set_target(game_target_t *target, pid_t pid, const char *name,
           const char *token, int title_known) {
  memset(target, 0, sizeof(*target));
  target->pid = pid;
  target->title_known = title_known;
  if(name) {
    copy_ascii_field(target->process_name, sizeof(target->process_name),
                     (const uint8_t *)name, PROCESS_NAME_SIZE - 1);
  }
  if(title_known && token) {
    copy_ascii_field(target->title_token, sizeof(target->title_token),
                     (const uint8_t *)token, TITLE_TOKEN_SIZE - 1);
  }
}

/* Return -1=query failure, 0=none, 1=one target, 2=ambiguous. */
static int
find_running_game(game_target_t *target, scan_stats_t *stats) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  uint8_t *buf = NULL;
  game_target_t only_eboot;
  game_target_t only_titled_eboot;
  game_target_t only_title;
  int titled_eboot_candidates = 0;

  memset(target, 0, sizeof(*target));
  memset(stats, 0, sizeof(*stats));
  memset(&only_eboot, 0, sizeof(only_eboot));
  memset(&only_titled_eboot, 0, sizeof(only_titled_eboot));
  memset(&only_title, 0, sizeof(only_title));

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
    uint8_t *end = buf + buf_size;
    int struct_size;
    pid_t pid;
    char process_name[PROCESS_NAME_SIZE];
    char command_name[PROCESS_NAME_SIZE];
    char title_token[TITLE_TOKEN_SIZE];
    char process_path[PROCESS_PATH_SIZE];
    int title_known;
    int eboot_named;
    int eboot_path = 0;

    if((size_t)(end - ptr) < sizeof(struct_size)) {
      free(buf);
      return -1;
    }
    memcpy(&struct_size, ptr, sizeof(struct_size));
    if(struct_size <= 0 || (size_t)struct_size > (size_t)(end - ptr) ||
       (size_t)struct_size <= PS5_KINFO_COMM_OFFSET ||
       (size_t)struct_size < PS5_KINFO_PID_OFFSET + sizeof(pid)) {
      free(buf);
      return -1;
    }

    memcpy(&pid, ptr + PS5_KINFO_PID_OFFSET, sizeof(pid));
    memset(process_name, 0, sizeof(process_name));
    copy_ascii_field(process_name, sizeof(process_name),
                     ptr + PS5_KINFO_TDNAME_OFFSET,
                     (size_t)struct_size - PS5_KINFO_TDNAME_OFFSET);
    memset(command_name, 0, sizeof(command_name));
    copy_ascii_field(command_name, sizeof(command_name),
                     ptr + PS5_KINFO_COMM_OFFSET,
                     (size_t)struct_size - PS5_KINFO_COMM_OFFSET);
    ptr += struct_size;
    stats->processes++;

    if(pid <= 0 || pid == getpid()) {
      continue;
    }
    title_known = !read_title_token(pid, title_token);
    eboot_named = starts_with_eboot(process_name, sizeof(process_name)) ||
                  starts_with_eboot(command_name, sizeof(command_name));
    if(!starts_with_eboot(process_name, sizeof(process_name)) &&
       starts_with_eboot(command_name, sizeof(command_name))) {
      memcpy(process_name, command_name, sizeof(process_name));
    }
    if(!eboot_named) {
      eboot_path = !read_process_path(pid, process_path) &&
                   path_is_eboot(process_path);
    }

    if(eboot_named || eboot_path || title_known) {
      klog_printf("[PS5 TROPHY UNLOCKER] scan pid=%d name=%s token=%s "
                  "name_eboot=%d path_eboot=%d\n",
                  pid, process_name[0] ? process_name : "<empty>",
                  title_known ? title_token : "<unknown>",
                  eboot_named, eboot_path);
    }

    if(eboot_named || eboot_path) {
      stats->eboot_candidates++;
      set_target(&only_eboot, pid, process_name, title_token, title_known);
      if(title_known) {
        titled_eboot_candidates++;
        set_target(&only_titled_eboot, pid, process_name, title_token, 1);
      }
    }
    if(title_known) {
      stats->title_candidates++;
      set_target(&only_title, pid, process_name, title_token, 1);
    }
  }
  free(buf);

  if(titled_eboot_candidates == 1) {
    memcpy(target, &only_titled_eboot, sizeof(*target));
    return 1;
  }
  if(titled_eboot_candidates > 1) {
    return 2;
  }
  if(stats->eboot_candidates == 1) {
    memcpy(target, &only_eboot, sizeof(*target));
    return 1;
  }
  if(stats->eboot_candidates > 1) {
    return 2;
  }
  if(stats->title_candidates == 1) {
    memcpy(target, &only_title, sizeof(*target));
    return 1;
  }
  if(stats->title_candidates > 1) {
    return 2;
  }
  return 0;
}

static int
find_shellcore(pid_t *shellcore_pid) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  uint8_t *buf = NULL;
  pid_t found = 0;
  int matches = 0;

  *shellcore_pid = 0;
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
    uint8_t *end = buf + buf_size;
    int struct_size;
    pid_t pid;
    char thread_name[PROCESS_NAME_SIZE];
    char command_name[PROCESS_NAME_SIZE];
    char process_path[PROCESS_PATH_SIZE];
    int is_match = 0;

    if((size_t)(end - ptr) < sizeof(struct_size)) {
      matches = -1;
      break;
    }
    memcpy(&struct_size, ptr, sizeof(struct_size));
    if(struct_size <= 0 || (size_t)struct_size > (size_t)(end - ptr) ||
       (size_t)struct_size <= PS5_KINFO_COMM_OFFSET ||
       (size_t)struct_size < PS5_KINFO_PID_OFFSET + sizeof(pid)) {
      matches = -1;
      break;
    }
    memcpy(&pid, ptr + PS5_KINFO_PID_OFFSET, sizeof(pid));
    memset(thread_name, 0, sizeof(thread_name));
    copy_ascii_field(thread_name, sizeof(thread_name),
                     ptr + PS5_KINFO_TDNAME_OFFSET,
                     (size_t)struct_size - PS5_KINFO_TDNAME_OFFSET);
    memset(command_name, 0, sizeof(command_name));
    copy_ascii_field(command_name, sizeof(command_name),
                     ptr + PS5_KINFO_COMM_OFFSET,
                     (size_t)struct_size - PS5_KINFO_COMM_OFFSET);
    ptr += struct_size;

    if(pid <= 0) {
      continue;
    }
    if(app_info_has_title(pid, SHELLCORE_TITLE_ID) ||
       starts_with_ci(thread_name, "SceShellCore") ||
       starts_with_ci(command_name, "SceShellCore")) {
      is_match = 1;
    } else if(!read_process_path(pid, process_path) &&
              path_is_shellcore(process_path)) {
      is_match = 1;
    }
    if(is_match) {
      found = pid;
      matches++;
      klog_printf("[PS5 TROPHY UNLOCKER] SceShellCore candidate pid=%d "
                  "thread=%s comm=%s\n", pid, thread_name, command_name);
    }
  }
  free(buf);

  if(matches == 1) {
    *shellcore_pid = found;
    return 1;
  }
  return matches < 0 ? -1 : matches;
}

static uint32_t
current_sdk_version(void) {
  uint32_t version = 0;
  size_t size = sizeof(version);

  if(sysctlbyname("kern.sdk_version", &version, &size, NULL, 0) ||
     size != sizeof(version)) {
    return 0;
  }
  return version;
}

static int
target_still_matches(const game_target_t *target) {
  game_target_t current;
  scan_stats_t stats;

  if(find_running_game(&current, &stats) != 1 || current.pid != target->pid) {
    return 0;
  }
  if(target->title_known &&
     (!current.title_known || strcmp(target->title_token,
                                     current.title_token))) {
    return 0;
  }
  return 1;
}

static int
resolve_trophy_symbols(pid_t pid, trophy_symbols_t *symbols) {
  memset(symbols, 0, sizeof(*symbols));

  for(int retry = 0; retry < 12; retry++) {
    if(!kernel_dynlib_handle(pid, "libSceNpTrophy2.sprx",
                             &symbols->trophy2_handle) &&
       symbols->trophy2_handle) {
      break;
    }
    symbols->trophy2_handle = 0;
    usleep(250000);
  }
  if(!symbols->trophy2_handle) {
    return -1;
  }

  symbols->create_context = kernel_dynlib_dlsym(
      pid, symbols->trophy2_handle, "sceNpTrophy2CreateContext");
  symbols->create_handle = kernel_dynlib_dlsym(
      pid, symbols->trophy2_handle, "sceNpTrophy2CreateHandle");
  symbols->destroy_handle = kernel_dynlib_dlsym(
      pid, symbols->trophy2_handle, "sceNpTrophy2DestroyHandle");
  symbols->get_game_info = kernel_dynlib_dlsym(
      pid, symbols->trophy2_handle, "sceNpTrophy2GetGameInfo");
  symbols->debug_unlock = kernel_dynlib_dlsym(
      pid, symbols->trophy2_handle,
      "sceNpTrophy2SystemDebugUnlockTrophy");

  klog_printf("[PS5 TROPHY UNLOCKER] Trophy2 handle=%u create_ctx=0x%lx "
              "create_handle=0x%lx destroy_handle=0x%lx info=0x%lx "
              "debug_unlock=0x%lx\n",
              symbols->trophy2_handle,
              (unsigned long)symbols->create_context,
              (unsigned long)symbols->create_handle,
              (unsigned long)symbols->destroy_handle,
              (unsigned long)symbols->get_game_info,
              (unsigned long)symbols->debug_unlock);

  if(!symbols->create_context || !symbols->create_handle ||
     !symbols->destroy_handle || !symbols->get_game_info ||
     !symbols->debug_unlock) {
    return -1;
  }
  return 0;
}

static int
read_game_info(pid_t pid, intptr_t fn, int32_t context_id,
               int32_t handle_id, intptr_t scratch,
               trophy_game_info_t *info) {
  uint8_t details[GAME_DETAILS_SIZE];
  uint8_t data[GAME_DATA_SIZE];
  long rc;

  memset(details, 0, sizeof(details));
  memset(data, 0, sizeof(data));
  memset(info, 0, sizeof(*info));
  if(pt_copyin(pid, details, scratch + SCRATCH_GAME_DETAILS,
               sizeof(details)) ||
     pt_copyin(pid, data, scratch + SCRATCH_GAME_DATA, sizeof(data))) {
    return -1;
  }

  rc = pt_call(pid, fn,
               (intptr_t)context_id,
               (intptr_t)handle_id,
               scratch + SCRATCH_GAME_DETAILS,
               scratch + SCRATCH_GAME_DATA,
               (intptr_t)0,
               (intptr_t)0);
  if((int32_t)rc < 0 ||
     pt_copyout(pid, scratch + SCRATCH_GAME_DETAILS,
                details, sizeof(details)) ||
     pt_copyout(pid, scratch + SCRATCH_GAME_DATA, data, sizeof(data))) {
    return -1;
  }

  memcpy(&info->total, details + 4, sizeof(info->total));
  memcpy(&info->unlocked, data, sizeof(info->unlocked));
  if(!info->total || info->total > TROPHY2_MAX_IDS ||
     info->unlocked > info->total) {
    return -1;
  }
  return 0;
}

static int
find_active_trophy_context(pid_t pid, const trophy_symbols_t *symbols,
                           int32_t handle_id, intptr_t scratch,
                           int32_t *context_id,
                           trophy_game_info_t *game_info) {
  uintptr_t lib_base;
  uintptr_t table_address;
  uint64_t table = 0;

  if((uintptr_t)symbols->create_context < TROPHY2_CREATE_CONTEXT_OFFSET) {
    return -1;
  }
  lib_base = (uintptr_t)symbols->create_context -
             TROPHY2_CREATE_CONTEXT_OFFSET;
  table_address = lib_base + TROPHY2_STATE_OFFSET;
  if(pt_copyout(pid, (intptr_t)table_address, &table, sizeof(table)) ||
     table < 0x10000ul || table >= 0x800000000000ul) {
    klog_printf("[PS5 TROPHY UNLOCKER] invalid Trophy2 state table "
                "base=0x%lx state@0x%lx value=0x%lx\n",
                (unsigned long)lib_base, (unsigned long)table_address,
                (unsigned long)table);
    return -1;
  }

  for(uintptr_t slot = TROPHY2_SLOT_FIRST;
      slot < TROPHY2_SLOT_END;
      slot += TROPHY2_SLOT_STEP) {
    uint64_t node = 0;
    uint8_t active = 0;
    int32_t candidate = 0;
    trophy_game_info_t candidate_info;

    if(pt_copyout(pid, (intptr_t)(table + slot), &node, sizeof(node)) ||
       node < 0x10000ul || node >= 0x800000000000ul) {
      continue;
    }
    if(pt_copyout(pid, (intptr_t)(node + TROPHY2_NODE_ACTIVE_OFFSET),
                  &active, sizeof(active)) || !active ||
       pt_copyout(pid, (intptr_t)(node + TROPHY2_NODE_CONTEXT_OFFSET),
                  &candidate, sizeof(candidate)) || candidate <= 0) {
      continue;
    }

    if(!read_game_info(pid, symbols->get_game_info, candidate,
                       handle_id, scratch, &candidate_info)) {
      *context_id = candidate;
      *game_info = candidate_info;
      klog_printf("[PS5 TROPHY UNLOCKER] active ctx=%d trophies=%u "
                  "unlocked=%u slot=0x%lx\n", candidate,
                  candidate_info.total, candidate_info.unlocked,
                  (unsigned long)slot);
      return 0;
    }
  }
  return -1;
}

static int
restore_shellcore(pid_t pid, intptr_t address,
                  const uint8_t original[SHELLCORE_PATCH_SIZE]) {
  uint8_t verify[SHELLCORE_PATCH_SIZE];

  for(int retry = 0; retry < 3; retry++) {
    memset(verify, 0, sizeof(verify));
    if(!kernel_proc_copyin(pid, original, address, SHELLCORE_PATCH_SIZE) &&
       !kernel_proc_copyout(pid, address, verify, sizeof(verify)) &&
       !memcmp(verify, original, sizeof(verify))) {
      return 0;
    }
    usleep(25000);
  }
  return -1;
}

static int
resolve_shellcore_auth_address(pid_t pid, intptr_t runtime_base,
                               intptr_t *address,
                               uint8_t original[SHELLCORE_PATCH_SIZE]) {
  static const uintptr_t image_bases[] = {
    SHELLCORE_IMAGE_BASE,
    0x00000000ul
  };

  *address = 0;
  memset(original, 0, SHELLCORE_PATCH_SIZE);
  for(size_t i = 0; i < sizeof(image_bases) / sizeof(image_bases[0]); i++) {
    intptr_t candidate = runtime_base +
                         (SHELLCORE_AUTH_VA - image_bases[i]);
    uint8_t bytes[SHELLCORE_PATCH_SIZE];

    memset(bytes, 0, sizeof(bytes));
    if(!kernel_proc_copyout(pid, candidate, bytes, sizeof(bytes)) &&
       !memcmp(bytes, shellcore_original, sizeof(bytes))) {
      *address = candidate;
      memcpy(original, bytes, sizeof(bytes));
      klog_printf("[PS5 TROPHY UNLOCKER] ShellCore image base "
                  "0x%lx verified, auth@0x%lx\n",
                  (unsigned long)image_bases[i],
                  (unsigned long)candidate);
      return 0;
    }
    klog_printf("[PS5 TROPHY UNLOCKER] ShellCore image base "
                "0x%lx signature miss @0x%lx: "
                "%02x %02x %02x %02x %02x %02x\n",
                (unsigned long)image_bases[i],
                (unsigned long)candidate,
                bytes[0], bytes[1], bytes[2],
                bytes[3], bytes[4], bytes[5]);
  }
  return -1;
}

int
main(void) {
  game_target_t target;
  scan_stats_t stats;
  trophy_symbols_t symbols;
  trophy_game_info_t before;
  trophy_game_info_t after;
  uint32_t sdk_version;
  pid_t shellcore_pid = 0;
  intptr_t shellcore_base = 0;
  intptr_t shellcore_auth = 0;
  uint8_t auth_original[SHELLCORE_PATCH_SIZE];
  uint8_t auth_verify[SHELLCORE_PATCH_SIZE];
  intptr_t scratch = 0;
  int32_t handle_id = 0;
  int32_t context_id = 0;
  int attached = 0;
  int patch_written = 0;
  int restore_failed = 0;
  int unlock_accepted = 0;
  int unlock_rejected = 0;
  int result = 39;
  int matches;

  memset(&before, 0, sizeof(before));
  memset(&after, 0, sizeof(after));
  memset(auth_original, 0, sizeof(auth_original));
  memset(auth_verify, 0, sizeof(auth_verify));

  klog_puts("[PS5 TROPHY UNLOCKER] direct launcher v2.0.0 starting");

  sdk_version = current_sdk_version();
  if(sdk_version < FW_1001_MIN || sdk_version > FW_1001_MAX) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: firmware 10.01 required\n"
           "Detected SDK: 0x%08x\nNo memory was changed", sdk_version);
    return 20;
  }

  matches = find_running_game(&target, &stats);
  if(matches < 0) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: process scan failed\n"
           "No memory was changed");
    return 21;
  }
  if(matches == 0) {
    notify("PS5 TROPHY UNLOCKER\nGAME NOT READY\n"
           "Open the game and wait for its main menu\n"
           "Scanned: eboot=%d title=%d",
           stats.eboot_candidates, stats.title_candidates);
    return 22;
  }
  if(matches > 1) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: multiple game candidates\n"
           "Close suspended games and retry\nNo memory was changed");
    return 23;
  }

  matches = find_shellcore(&shellcore_pid);
  if(matches != 1 || shellcore_pid <= 0) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: SceShellCore not found uniquely\n"
           "No memory was changed");
    return 24;
  }
  shellcore_base = kernel_dynlib_mapbase_addr(shellcore_pid, 0);
  if(shellcore_base <= 0 || (shellcore_base & 0xfff)) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: invalid ShellCore base\n"
           "No memory was changed");
    return 25;
  }
  if(resolve_shellcore_auth_address(shellcore_pid, shellcore_base,
                                    &shellcore_auth, auth_original)) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: 10.01 safety signature mismatch\n"
           "Known layouts were checked\nNo memory was changed");
    return 26;
  }

  if(resolve_trophy_symbols(target.pid, &symbols)) {
    notify("PS5 TROPHY UNLOCKER\nGAME NOT READY\n"
           "Trophy2 is not loaded yet\nWait in the game menu and retry\n"
           "No memory was changed");
    return 27;
  }

  notify("PS5 TROPHY UNLOCKER\nTARGET: %s (PID %d)\n"
         "Firmware 10.01 verified\nReading active Trophy2 context...",
         target.title_known ? target.title_token : "eboot.bin", target.pid);

  if(pt_attach(target.pid)) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: cannot attach to game\n"
           "No memory was changed");
    return 28;
  }
  attached = 1;

  if(!target_still_matches(&target)) {
    result = 29;
    goto cleanup;
  }

  scratch = pt_mmap(target.pid, 0, SCRATCH_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
  if(scratch == (intptr_t)-1 || !scratch) {
    scratch = 0;
    result = 30;
    goto cleanup;
  }

  {
    int32_t zero = 0;
    long rc;

    if(pt_copyin(target.pid, &zero, scratch + SCRATCH_HANDLE_ID,
                 sizeof(zero))) {
      result = 31;
      goto cleanup;
    }
    rc = pt_call(target.pid, symbols.create_handle,
                 scratch + SCRATCH_HANDLE_ID,
                 (intptr_t)0, (intptr_t)0, (intptr_t)0,
                 (intptr_t)0, (intptr_t)0);
    if((int32_t)rc < 0 ||
       pt_copyout(target.pid, scratch + SCRATCH_HANDLE_ID,
                  &handle_id, sizeof(handle_id)) || handle_id <= 0) {
      result = 31;
      goto cleanup;
    }
  }

  if(find_active_trophy_context(target.pid, &symbols, handle_id,
                                scratch, &context_id, &before)) {
    result = 32;
    goto cleanup;
  }

  if(before.unlocked == before.total) {
    after = before;
    result = 0;
    goto cleanup;
  }

  notify("PS5 TROPHY UNLOCKER\nGAME DETECTED: %s\n"
         "Trophies: %u total / %u already unlocked\n"
         "Applying temporary 10.01 authorization...",
         target.title_known ? target.title_token : "eboot.bin",
         before.total, before.unlocked);

  /* Last-second process/base/signature validation before the only write. */
  if(kernel_dynlib_mapbase_addr(shellcore_pid, 0) != shellcore_base ||
     kernel_proc_copyout(shellcore_pid, shellcore_auth,
                         auth_verify, sizeof(auth_verify)) ||
     memcmp(auth_verify, auth_original, sizeof(auth_verify))) {
    result = 33;
    goto cleanup;
  }

  if(kernel_proc_copyin(shellcore_pid, shellcore_patch,
                        shellcore_auth, sizeof(shellcore_patch))) {
    result = 34;
    goto cleanup;
  }
  patch_written = 1;
  memset(auth_verify, 0, sizeof(auth_verify));
  if(kernel_proc_copyout(shellcore_pid, shellcore_auth,
                         auth_verify, sizeof(auth_verify)) ||
     memcmp(auth_verify, shellcore_patch, sizeof(auth_verify))) {
    result = 34;
    goto cleanup;
  }

  for(uint32_t trophy_id = 0; trophy_id < TROPHY2_MAX_IDS; trophy_id++) {
    uint32_t spec[2] = {1u, trophy_id};
    long rc;

    if(pt_copyin(target.pid, spec, scratch + SCRATCH_UNLOCK_SPEC,
                 sizeof(spec))) {
      unlock_rejected++;
      continue;
    }
    rc = pt_call(target.pid, symbols.debug_unlock,
                 (intptr_t)context_id,
                 (intptr_t)handle_id,
                 scratch + SCRATCH_UNLOCK_SPEC,
                 (intptr_t)0, (intptr_t)0, (intptr_t)0);
    if((int32_t)rc >= 0) {
      unlock_accepted++;
    } else {
      unlock_rejected++;
    }
    usleep(25000);
  }

  /* The bypass is never left active while waiting or cleaning up. */
  if(restore_shellcore(shellcore_pid, shellcore_auth, auth_original)) {
    restore_failed = 1;
    result = 35;
    goto cleanup;
  }
  patch_written = 0;

  klog_printf("[PS5 TROPHY UNLOCKER] direct calls accepted=%d rejected=%d\n",
              unlock_accepted, unlock_rejected);

  if(!unlock_accepted) {
    result = 36;
    goto cleanup;
  }

  after = before;
  for(int retry = 0; retry < 20; retry++) {
    trophy_game_info_t latest;
    if(!read_game_info(target.pid, symbols.get_game_info, context_id,
                       handle_id, scratch, &latest)) {
      after = latest;
      if(after.unlocked >= after.total) {
        break;
      }
    }
    usleep(500000);
  }

  result = after.unlocked >= after.total ? 0 : 37;

cleanup:
  if(patch_written) {
    if(restore_shellcore(shellcore_pid, shellcore_auth, auth_original)) {
      restore_failed = 1;
    }
  }

  if(attached && handle_id > 0) {
    (void)pt_call(target.pid, symbols.destroy_handle,
                  (intptr_t)handle_id,
                  (intptr_t)0, (intptr_t)0, (intptr_t)0,
                  (intptr_t)0, (intptr_t)0);
  }
  if(attached && scratch) {
    (void)pt_munmap(target.pid, scratch, SCRATCH_SIZE);
  }
  if(attached) {
    (void)pt_detach(target.pid, SIGCONT);
    (void)kill(target.pid, SIGCONT);
  }

  if(restore_failed) {
    notify("PS5 TROPHY UNLOCKER\nCRITICAL: ShellCore restore failed\n"
           "REBOOT THE CONSOLE before another game\n"
           "Result code: %d", result);
    return 38;
  }
  if(result == 0) {
    notify("PS5 TROPHY UNLOCKER\nCOMPLETE: %s\n"
           "%u / %u trophies verified unlocked\n"
           "Temporary authorization restored",
           target.title_known ? target.title_token : "eboot.bin",
           after.unlocked, after.total);
    return 0;
  }
  if(result == 29) {
    notify("PS5 TROPHY UNLOCKER\nSTOP: running game changed\n"
           "No ShellCore patch was applied");
  } else if(result == 32) {
    notify("PS5 TROPHY UNLOCKER\nGAME NOT READY\n"
           "No active Trophy2 context was found\n"
           "Open the trophy screen, return to game, then retry\n"
           "No ShellCore patch was applied");
  } else if(result == 37) {
    notify("PS5 TROPHY UNLOCKER\nPARTIAL RESULT: %s\n"
           "%u / %u verified unlocked\n"
           "Accepted calls: %d\nTemporary authorization restored",
           target.title_known ? target.title_token : "eboot.bin",
           after.unlocked, after.total, unlock_accepted);
  } else {
    notify("PS5 TROPHY UNLOCKER\nSTOPPED SAFELY (code %d)\n"
           "Accepted=%d rejected=%d\n"
           "ShellCore is restored", result,
           unlock_accepted, unlock_rejected);
  }
  return result;
}
