# Third-party notices

## ps5-payload-dev/elfldr

`pt.c`, `pt.h`, `notify.c`, and `notify.h` are derived from
`ps5-payload-dev/elfldr` commit
`699e8bcff03e91e8d6ca6eba281af25c5a58d8c2` and are distributed under
GPL-3.0-or-later. See `LICENSE`.

## ps5-payload-dev SDK

The payload is built against ps5-payload-dev SDK v0.42 (source commit
`4eb701204fc3f8d31e84cf8ca272974e2be9c867`). The SDK is GPL-3.0-or-later
except where its individual files state a BSD license.

## PS5 PHU Trophy System

The firmware 10.01 Trophy2 client-state layout, SceShellCore authorization
address, expected prologue, temporary patch bytes and direct unlock call
contract are adapted from Team PHU's `PS5-PHU-Trophy-System`, source commit
`ae29e8a7b4b458233c4859c1a33903b814c5010f`.

MIT License

Copyright (c) 2026 Arksama (Team PHU)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Legacy trophy engine archive

`engine/trophy_unlocker_engine.elf` is the non-debug `PS5 Unlocker.elf` from
the user-supplied `Trophee unlocker beta.zip`.

- SHA-256: `c4fc9a036b324f73f8b6e2b6465e1d6550f6723855d70a82e2ade82343f282c9`
- Original size: 166456 bytes

No C/C++ source or license for this engine was present in that archive. It is
kept only as the original user-supplied reference input. Version 2.0 does not
embed, inject or link this binary and does not represent it as source code
created by SpiderKit.
