# SpiderKit — PS5 WebKit UI Edition

SpiderKit is a visual redesign of Jordy's `slopkit` host. The exploit chain,
firmware offsets, kernel stages, loader flow and bundled binaries are retained
from the upstream project; the landing page, runtime monitor and payload menu
have been redesigned for a clearer controller-first experience.

## Visual changes

- User-provided Spider mask icon, optimized locally for the PS5 browser
- Dark red/blue launch screen with a focused `BRAND NEW JAILBREAK` action
- Six staged payloads visible but disabled on the initial page
- Five-step runtime rail: WebKit, kernel, kernel R/W, root and ELF loader
- Full-screen `/payloads/` directory with ready, sending, sent and failed states
- Directional controller navigation in a separate, PS5-friendly document
- No external fonts, trackers or visual dependencies
- All interface assets are stored locally

## Runtime flow

The `RUN` action preserves the upstream URL and parameters:

`slopkit/poops.html?go=1&trigger=netcontrol&payload=1&v=35`

After a successful chain, the runtime opens `payloads/index.html` inside the
same live page. The separate directory document keeps the exploit runtime in
memory while sending one selected ELF at a time to `127.0.0.1:9021`.

The initial page only previews the six payloads in a disabled state. They are
made selectable only after the runtime confirms that the ELF loader is online.

Opening `payloads/` directly is a safe visual preview only. A standalone web
page cannot send a raw ELF to the loader without the live parent runtime.

## Stability guardrails

- A second run is blocked while the first is still active.
- The landing page no longer clears the one-shot latch automatically.
- Legacy `auto=1` links can no longer clear a set latch automatically because
  a browser page cannot reliably prove that the console was rebooted.
- Runtime HUD updates and decorative compositing are suspended during race
  windows, then restored after the timing-sensitive section finishes.
- The runtime icon is a separate 192 px asset to reduce decoded image memory.
- The payload directory uses a separate same-origin frame and a single-column
  fixed-height list. This avoids repainting the exploit document's card tree
  after the kernel stages and avoids Grid, nested Flexbox and focus scrolling.

These changes do not alter exploit primitives, firmware offsets, race counts,
timeouts, syscall sequences, kernel-write targets or bundled payloads. A kernel
exploit can never be made panic-proof; reboot whenever the runtime requests it
and never retry an armed run in the same boot after kernel state was changed.

## Hosting

This is a static project. Serve the repository root through HTTP or publish the
root to GitHub Pages. Do not open the files directly with a `file://` URL.
The directory page must keep the exact path `payloads/index.html`; a browser
download renamed to `index(1).html` will not replace the page served by GitHub.

## Attribution

Original exploit project: Jordy / `jordyidk/slopkit`.

Upstream credits: Egy, Sonic, Yenyen, Zeco, Gezine, Echostretch, Ufm42,
TheFloW, John Tornblom, Flatz and PS5 R&D Discord.

Spider interface: zer0day.

The interface is an independent visual redesign and is not affiliated with or
endorsed by Sony, Marvel or Disney. The included mask icon was supplied for
this build; verify that you have the necessary rights before public reuse.

## Important notice

The upstream repository did not include a license when this visual derivative
was prepared. Preserve all attribution and obtain the upstream author's
permission before redistribution or public hosting. Use only on hardware and
firmware you are authorized to test. Kernel state changes may require a reboot.
