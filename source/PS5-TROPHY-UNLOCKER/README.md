# PS5 TROPHY UNLOCKER

This directory contains the reproducible source for SpiderKit's firmware
10.01 direct Trophy2 payload. Version 2.0 no longer embeds or injects the
binary-only beta engine and does not treat a posted UDS event as proof that a
trophy changed.

## Runtime flow

1. Verify that `kern.sdk_version` is exactly in the PS5 10.01 range.
2. Select one unambiguous running `eboot.bin` and revalidate the same PID after
   attaching.
3. Resolve `libSceNpTrophy2.sprx` inside that game and reuse only an active
   context whose `sceNpTrophy2GetGameInfo` call succeeds.
4. Read the real total/unlocked counts before changing anything.
5. Locate SceShellCore by `NPXS40082`, resolve either supported runtime image
   layout and require the exact six-byte 10.01 prologue before writing.
6. Apply the six-byte debug-authorization patch temporarily, call
   `sceNpTrophy2SystemDebugUnlockTrophy` for the active context, and restore the
   original bytes immediately after the calls.
7. Re-read game information and report `COMPLETE` only when the system reports
   `unlocked == total`.

Every preflight mismatch aborts before the ShellCore write. The original bytes
are verified after restoration; a restoration failure produces a critical
notification asking for an immediate console reboot.

## Build

Install the ps5-payload-dev SDK, then run:

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make
```

The output is `PS5-TROPHY-UNLOCKER.ELF`. SpiderKit publishes the identical
bytes as `payloads/PS5 TROPHY UNLOCKER.ELF`.

## Use

1. Complete the jailbreak and start the compatible `kstuff.elf`.
2. Launch one game and wait until its main menu and Trophy2 context are ready.
3. Return to SpiderKit and select **PS5 TROPHY UNLOCKER** once.
4. Read the PS5 notification. Only `COMPLETE: <title> — N / N trophies verified
   unlocked` is a verified complete result.

This build is intentionally locked to firmware 10.01. It will not guess
offsets on another firmware. It changes local trophy state irreversibly; keep
the console offline and do not sync artificial trophy unlocks to PSN.

The 10.01 Trophy2 context layout, ShellCore address and direct debug-call
research are adapted from Team PHU's MIT-licensed PS5 PHU Trophy System. See
`THIRD_PARTY_NOTICES.md`. The legacy beta engine remains in `engine/` only as
an archived user-supplied input and is not linked into version 2.0.
