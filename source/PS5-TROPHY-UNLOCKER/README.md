# PS5 TROPHY UNLOCKER

This directory contains the reproducible source for SpiderKit's one-shot
launcher. The launcher embeds the supplied beta trophy engine and injects it
only into one unambiguous running game process.

## Safety gates

- Accepts only a process named `eboot.bin` with a valid `PPSA#####`,
  `PPSB#####`, or `CUSA#####` title ID.
- Refuses to run when no game or more than one matching game is present.
- Validates the embedded ELF header and program-header bounds before attach.
- Freezes the selected PID and verifies its title ID a second time before
  injection, closing the PID-reuse/title-switch race.
- Writes `mode=all` only after target validation. Failed injection removes the
  temporary config.
- Uses the current ps5-payload-dev ELF loader path and restores the target's
  jail, root directory, capabilities, authorization ID, and effective UID.

## Build

Install the official ps5-payload-dev SDK, then run:

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make
```

The output is `PS5-TROPHY-UNLOCKER.ELF`. SpiderKit publishes the same bytes as
`payloads/PS5 TROPHY UNLOCKER.ELF` so the user-facing name matches the menu.

## Runtime

1. Complete the jailbreak and start `kstuff.elf`.
2. Launch the game and leave it running.
3. Return to SpiderKit and select **PS5 TROPHY UNLOCKER**.
4. The launcher selects exactly that game's Title ID, injects the supplied
   engine into its `eboot.bin`, and requests `mode=all`.

The operation modifies local trophy state and is not reversible by this
payload. Keep the console offline and do not sync artificial trophy unlocks to
PSN.

The embedded beta engine was supplied as a binary-only ELF; its C/C++ source
was not present in the uploaded archive. Consequently, this project wraps and
validates that engine rather than claiming to have rebuilt it from source.
