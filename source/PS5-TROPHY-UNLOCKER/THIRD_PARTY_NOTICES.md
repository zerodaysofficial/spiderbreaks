# Third-party notices

## ps5-payload-dev/elfldr

`elfldr.c`, `elfldr.h`, `pt.c`, `pt.h`, `notify.c`, `notify.h`, and `log.h` are
derived from `ps5-payload-dev/elfldr` commit
`699e8bcff03e91e8d6ca6eba281af25c5a58d8c2` and are distributed under
GPL-3.0-or-later. See `LICENSE`.

## ps5-payload-dev SDK

The payload is built against ps5-payload-dev SDK v0.42 (source commit
`4eb701204fc3f8d31e84cf8ca272974e2be9c867`). The SDK is GPL-3.0-or-later
except where its individual files state a BSD license.

## Embedded trophy engine

`engine/trophy_unlocker_engine.elf` is the non-debug `PS5 Unlocker.elf` from
the user-supplied `Trophee unlocker beta.zip`.

- SHA-256: `c4fc9a036b324f73f8b6e2b6465e1d6550f6723855d70a82e2ade82343f282c9`
- Original size: 166456 bytes

No C/C++ source or license for this engine was present in that archive. It is
embedded as an independent data payload and is not represented as source code
created by SpiderKit.
