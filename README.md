# SpiderKit — Spider Protocol UI41

SpiderKit is a visual redesign of Jordy's `slopkit` host. The exploit chain,
firmware offsets, kernel stages, loader flow and bundled binaries are retained
from the upstream project; the landing page, runtime monitor and payload menu
have been redesigned for a clearer controller-first experience.

## Visual changes

- User-provided Spider mask icon, optimized locally for the PS5 browser
- Fully interactive Spider-Man-inspired red/blue launch experience
- Clickable mask with a Spider-Sense pulse and controller-first focus states
- Focused `BRAND NEW JAILBREAK` action and animated runtime handoff
- Eleven staged payloads visible but disabled on the initial page
- Five-step runtime rail: WebKit, kernel, kernel R/W, root and ELF loader
- Runtime progress bar driven by the real firmware-offset load and exploit marks
- In-page payload directory with ready, sending, sent and failed states
- Directional controller navigation in the post-exploit payload menu
- No external fonts, trackers or visual dependencies
- All interface assets are stored locally

## Runtime flow

The `RUN` action preserves the upstream URL and parameters:

`slopkit/poops.html?go=1&trigger=netcontrol&payload=1&v=41`

After a successful chain, the runtime exposes its payload menu directly inside
`slopkit/poops.html`, without navigating or creating a second frame. Selecting
one entry sends that ELF to `127.0.0.1:9021`.

The initial page only previews the eleven payloads in a disabled state. They are
made selectable only after the runtime confirms that the ELF loader is online.

The success screen switches to the local Spider-Man GIF only after the kernel
and loader stages finish. The five additional payloads are `shadowmountplus`,
`cheatrunner`, `ps5-backpork`, `pldmgr_v0.5.1` and the trophy launcher; the
sender limit is 12 MiB and data is still copied through the existing 64 KiB
staging buffer.

`payloads/PS5 TROPHY UNLOCKER.ELF` is a one-shot launcher built for this
package. It scans for exactly one running `eboot.bin`, accepts only a valid
`PPSA#####`, `PPSB#####` or `CUSA#####` Title ID, freezes and revalidates the
same PID, then injects the non-debug engine supplied in `Trophee unlocker
beta.zip`. No game, an ambiguous process list, a changed Title ID or an invalid
embedded ELF all cause a clean abort before the engine runs. The launcher
forces `mode=all`, so select it only while the intended game is running. Its
reproducible launcher source and GPL notices are in
`source/PS5-TROPHY-UNLOCKER/`.

`payloads/kstuff.elf` is the official [EchoStretch Kstuff Lite v1.09 release](https://github.com/EchoStretch/kstuff-lite/releases/tag/v1.09)
asset, not a locally rebuilt variant. Its expected SHA-256 is
`ec5212794dc6e44ee8e70fd0549abec6d3dac8c3e03ddbeafd9f869ffe97d4e8`.
Payload fetches include the UI41 asset revision and `no-store` so GitHub Pages
and the console browser do not silently reuse the previous Kstuff binary.

When the selected firmware file in `offsets/` really finishes loading, the HUD
shows its filename and advances the progress bar. Later percentages follow the
existing runtime markers for the WebKit primitive, kernel stages, kernel R/W,
root escape and confirmed ELF-loader completion; they are not a timer.

After confirmed loader success, a small success record is written to both
session storage and persistent local storage. Reopening the site therefore
goes directly to the saved payload deck and does not rerun the kernel exploit.
The original open runtime remains the only page with a live ROP sender. A truly
new or reloaded document can restore the payload list and success screen, but
cannot reconstruct that live JavaScript/ROP object safely, so sending is
disabled there. After a full console reboot, use `FULL REBOOT DONE? CLEAR SAVED
STATE` before starting a new jailbreak.

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
- The eleven payload controls live directly in the runtime document and use
  fixed-height block rows. No iframe or secondary page must be painted after
  the kernel stages.
- A successful payload session installs a navigation guard so an accidental
  refresh or Back action does not discard the live ROP sender. If the browser
  forcibly reloads the WebProcess, the success page and all eleven payloads are
  restored without another kernel exploit, but sending is disabled because
  JavaScript cannot safely reconstruct that lost live state.

These changes do not alter exploit primitives, firmware offsets, race counts,
timeouts, syscall sequences, kernel-write targets or bundled payloads. A kernel
exploit can never be made panic-proof; reboot whenever the runtime requests it
and never retry an armed run in the same boot after kernel state was changed.

The trophy launcher changes local trophy state and has no undo action. Keep the
console offline and do not sync artificial unlocks to PSN. The uploaded beta
archive did not contain the trophy engine's C/C++ source, so SpiderKit embeds
and validates the supplied non-debug binary rather than presenting it as a
source rebuild. Hardware behavior still depends on the console firmware, the
active jailbreak, Kstuff and the supplied engine.

## Hosting

This is a static project. Serve the repository root through HTTP or publish the
root to GitHub Pages. Do not open the files directly with a `file://` URL.
The directory page must keep the exact path `payloads/index.html`; a browser
download renamed to `index(1).html` will not replace the page served by GitHub.

## Attribution

Exploit research credits shown in the interface: Sonic Iso and Jordy.

Original exploit project: Jordy / `jordyidk/slopkit`.

Upstream credits: Egy, Sonic, Yenyen, Zeco, Gezine, Echostretch, Ufm42,
TheFloW, John Tornblom, Flatz and PS5 R&D Discord.

Spider interface: zer0day.

The interface is an independent visual redesign and is not affiliated with or
endorsed by Sony, Marvel or Disney. The included mask icon and success GIF were
supplied for this build; verify that you have the necessary rights before
public reuse.

## Important notice

The upstream repository did not include a license when this visual derivative
was prepared. Preserve all attribution and obtain the upstream author's
permission before redistribution or public hosting. Use only on hardware and
firmware you are authorized to test. Kernel state changes may require a reboot.
