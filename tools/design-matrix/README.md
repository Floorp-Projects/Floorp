# design-matrix

Screenshots and computed colours of every Floorp design × theme, so a Runtime
(Gecko) bump can be compared against the previous release before it ships. It
was written after the Gecko 159 bump, where regressions such as #3093 (toolbar
colour), #3095 (new tab shortcut icons), #3096 (LWT toolbox see-through in F11)
and #3100 (`:-moz-lwtheme` selectors) were only found by users.

## What it captures

For each design (`lepton`, `photon`, `protonfix`, `proton`, `fluerial`, set
through `floorp.design.configs` → `globalConfigs.userInterface`) and each theme:

| theme        | what it is                                                                 |
| ------------ | -------------------------------------------------------------------------- |
| `none`       | default theme (`default-theme@mozilla.org`)                                |
| `light`      | built-in Light (`firefox-compact-light@mozilla.org`)                       |
| `dark`       | built-in Dark (`firefox-compact-dark@mozilla.org`)                         |
| `lwt-header` | local test theme `fixtures/lwt-header`: frame colour + `theme_frame` image |
| `lwt-bottom` | local test theme `fixtures/lwt-bottom`: `additional_backgrounds`, bottom   |

For every combination the browser is started with a fresh temporary profile
(`-no-remote -profile <tmp> -marionette [-headless]`,
`MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1`) and these states are recorded:

| state           | screenshot | notes                                                                                                      |
| --------------- | ---------- | ---------------------------------------------------------------------------------------------------------- |
| `main`          | yes        | static fixture page `http://design-matrix.test/page.html`                                                  |
| `contextmenu`   | no         | content context menu; native popups are not in WebDriver screenshots, so only its computed styles are kept |
| `fullscreen`    | yes        | F11 (`window.fullScreen`) with the toolbox revealed (`FullScreen.showNavToolbox()`)                        |
| `clear-history` | yes        | Clear History dialog (`Sanitizer.showUI`)                                                                  |
| `newtab`        | yes        | `about:home` with Floorp Start off (Activity Stream)                                                       |

Screenshots are window-only (chrome-context `WebDriver:TakeScreenshot`), never
desktop captures. Each state also records computed `background-color`,
`background-image`, `color`, `opacity`, `display`, `visibility` and the box of
`:root`, `body`, `#navigator-toolbox`, `#TabsToolbar`, `#nav-bar`, the selected
tab's `.tab-background`, `#urlbar-background`, `#browser`, `#panel-sidebar-box`
and `#nora-statusbar`.

To keep runs reproducible the tool:

- routes all browser traffic to its own small proxy on `--port + 1`, which
  serves `fixtures/` as `http://design-matrix.test/` and refuses everything else
  (no network content, and the URL bar text does not depend on the checkout
  path);
- pins the OS colour scheme to light (`ui.systemUsesDarkTheme=0`), so theme
  `none` (the default theme follows the system) looks the same on every machine
  and does not turn into a copy of `dark` on a dark-mode machine; built-in Dark
  and the LWTs set their own colours;
- disables caret blinking, animations, welcome/what's-new pages, sponsored and
  remote content on `about:home`;
- masks the size labels in the Clear History dialog (cache and site-data sizes
  change between runs);
- takes screenshots until two consecutive captures are identical;
- checks, before recording, that the browser applied the requested design
  (`globalConfigs.userInterface`) and theme (`extensions.activeThemeID`, and
  `[lwtheme]` for the LWTs); a mismatch is a capture error, never a capture
  under the wrong label.

On Windows 11 headless, repeat runs of the same build changed 0 pixels and 0
style values across all 100 images (before the Clear History size labels were
masked, those labels were the only difference, about 260 pixels). Re-check this
on a new machine by running the same build twice and comparing.

## Running before and after a Runtime bump

Use absolute paths (`deno task` runs from the repository root). Nothing is
copied from the build; temporary profiles go to the OS temp directory (or
`--tmp-dir`) and are deleted after each combination. A full run (25
combinations, about 20 s each) takes about 8–8.5 minutes and writes about 6 MB.

```sh
# 1. Before the bump: capture the current release as the baseline.
deno task design-matrix run --exe /path/to/old/floorp.exe --out /tmp/dm-before

# 2. After the bump: capture the candidate and compare in one go.
deno task design-matrix run --exe /path/to/new/floorp.exe --out /tmp/dm-after --baseline /tmp/dm-before

# Re-compare two existing runs (e.g. with other limits).
deno task design-matrix compare --baseline /tmp/dm-before --current /tmp/dm-after
```

Record the baseline and the candidate on the same machine and screen settings.

Keep the baseline clean. A capture problem in the baseline (see exit code 2
below) makes every later comparison against it exit 2 as well, for as long as
the affected combinations are compared. Check the baseline run's own output (it
lists its capture problems and exits 2 if there are any) and capture it again
before relying on it.

Runs recorded before the colour scheme pin (`ui.systemUsesDarkTheme=0`) and the
narrowed Clear History masks were added (commit `47a975e`) are not comparable
with newer runs: on a dark-mode machine `none` was rendered dark, and the older
masks differ. Capture such baselines again.

Useful options: `--designs lepton,photon`, `--themes none,lwt-header`,
`--port 29850` (Marionette; the proxy uses the next port), `--headed`,
`--width 1280 --height 800`, `--tolerance 8`, `--max-pixels 16`,
`--threshold 0.002`, `--max-mask 0.02`, `--ignore-styles`, `--no-diff-images`.
`--help` lists them all.

An image fails when more than `--max-pixels` pixels (default 16) or more than
`--threshold` of its pixels (default 0.2%) changed. The pixel floor catches
small regressions such as a changed icon or a 1px border (a 1px line across the
window is over 1,000 pixels), which a ratio alone would let through. Same-build
runs change 0 pixels, so the default leaves room only for rare anti-aliasing
differences. Across a real Gecko bump, font and icon updates will exceed it in
many images: read the diff PNGs first, then re-run `compare` with a higher
`--max-pixels` to see what remains.

Exit codes:

- `0` no failing image, no style difference, no capture problem;
- `1` regressions found (failing images, or style differences unless
  `--ignore-styles`);
- `2` any error: bad arguments (including a numeric option that is not a whole
  number in range, such as `--port 28.5`, or a ratio outside 0–1), a missing,
  invalid or incomplete run directory, a busy port, or a capture problem in
  either run (a combination failed to start, the design or theme was not
  applied, a state was not reached, the context menu did not open, a screenshot
  never settled). Capture problems are listed in the report.
- `130` `run` was interrupted with Ctrl+C.

Only processes the tool started are stopped: first `Marionette:Quit`; if the
browser hangs, on Windows `taskkill /T` of its own launcher PID (kept alive with
`-wait-for-browser`), on Linux/macOS SIGKILL to that PID and every descendant
found with `pgrep -P`. Without `pgrep` on Linux/macOS only that PID is killed,
and the tool prints a warning, since its child processes may keep running.
Ctrl+C stops the current browser the same way, deletes its profile and the
temporary root, and starts nothing new.

`manifest.json` is replaced atomically (written to a temporary file, then
renamed) and carries `"complete": false` from the moment `run` starts until the
last combination is done. A run stopped by Ctrl+C, a crash or a killed process
therefore keeps `"complete": false`, and `compare` (and `run --baseline`)
refuses it with exit 2 instead of treating the missing combinations as a
deliberate `--designs`/`--themes` subset. Capture such a run again. Manifests
from before this field was added have no `complete` and are read as complete.

## Reading the report

`<out>/report/report.md` (and `report.json`) contain:

- **Result** — `PASS`, `FAIL` (regressions, exit 1) or `ERROR` (capture
  problems, exit 2).
- **Masked** — how many images had masked regions and how many pixels were
  excluded. Masks may cover at most `--max-mask` of an image (default 2%); if a
  layout change makes a masked region grow beyond that, the image is compared
  without masks and the report says so, so a growing mask cannot hide changes
  elsewhere in the image. Changes inside a mask that stays under the cap are not
  seen: today only the Clear History size labels (about 1% of the image) are
  masked, and a regression confined to them would not be reported.
- **Capture problems** — errors and per-state problems from either run, and
  states recorded in neither run.
- **Failing images** — `design/theme/state`, changed pixels and ratio, the
  bounding box of the changed pixels and a link to `diff/*.diff.png` (the
  current screenshot greyed out, changed pixels in magenta, masked regions dark
  grey). A pixel counts as changed when any RGBA channel differs by more than
  `--tolerance`. `size-mismatch` means the window or screenshot size changed;
  `missing` / `added` mean only one run has the screenshot.
- **Computed-style differences** — `design/theme/state/surface/property` with
  the baseline and current value. These point at the element that changed even
  when the pixels barely move. Some designs override a property with
  `!important`, so the same CSS change can show up in some designs and not in
  others: in the PR's demo, `#nav-bar { background-color: transparent }` changed
  `nav-bar/background-color` under Proton, Fluerial and the LWTs, while Lepton,
  Photon and ProtonFix with the default and built-in themes kept their own
  colour.
- **All images** — every comparison, including unchanged ones, with masked pixel
  counts.

A Runtime bump is expected to move some pixels (font rendering, icon updates).
Look at the diff PNGs: changes confined to the URL bar text or an icon are
usually upstream; whole toolbars changing colour, see-through toolboxes in
`fullscreen`, or blank tiles in `newtab` are the kind of regression this tool is
for. The per-run `manifest.json` also keeps informational notes, such as the
`userChrome.fullscreen.overlap` value seen in `fullscreen`.

## Files

- `mod.ts` — CLI (`run`, `compare`)
- `capture.ts` — browser launch, Marionette driving, fixture proxy
- `compare.ts` — pixel and value comparison, Markdown report
- `png.ts` — dependency-free PNG decode/encode
- `fixtures/` — static page and the two local test themes
- `compare.test.ts` — run by `deno task test:host` (and
  `deno task test:design-matrix`, which also runs the busy-port test that needs
  `--allow-net`)
