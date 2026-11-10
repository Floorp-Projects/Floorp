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
- disables caret blinking, animations, welcome/what's-new pages, sponsored and
  remote content on `about:home`;
- takes screenshots until two consecutive captures are identical.

Run-to-run noise on Windows 11 headless is reported in the PR that added this
tool; re-check it on a new machine by running the same build twice.

## Running before and after a Runtime bump

Use absolute paths (`deno task` runs from the repository root). Nothing is
copied from the build; temporary profiles go to the OS temp directory (or
`--tmp-dir`) and are deleted after each combination. A full run (25
combinations) takes about 10 minutes and writes about 15 MB.

```sh
# 1. Before the bump: capture the current release as the baseline.
deno task design-matrix run --exe /path/to/old/floorp.exe --out /tmp/dm-before

# 2. After the bump: capture the candidate and compare in one go.
deno task design-matrix run --exe /path/to/new/floorp.exe --out /tmp/dm-after --baseline /tmp/dm-before

# Re-compare two existing runs (e.g. with another threshold).
deno task design-matrix compare --baseline /tmp/dm-before --current /tmp/dm-after
```

Useful options: `--designs lepton,photon`, `--themes none,lwt-header`,
`--port 29850` (Marionette; the proxy uses the next port), `--headed`,
`--width 1280 --height 800`, `--tolerance 8`, `--threshold 0.002`,
`--ignore-styles`, `--no-diff-images`. `--help` lists them all.

Exit codes: `0` nothing above threshold, `1` regressions found, `2` usage or
capture error (a combination failed to start, a state could not be reached).

Only processes the tool started are stopped (`Marionette:Quit`, then a
`taskkill /T` of its own launcher PID if the browser hangs).

## Reading the report

`<out>/report/report.md` (and `report.json`) contain:

- **Result** — `FAIL` when any image changed more than `--threshold` of its
  pixels, or (unless `--ignore-styles`) any recorded computed value differs.
- **Images over threshold** — `design/theme/state`, the changed-pixel ratio, the
  bounding box of the changed pixels and a link to `diff/*.diff.png` (the
  current screenshot greyed out, changed pixels in magenta). A pixel counts as
  changed when any RGBA channel differs by more than `--tolerance`.
  `size-mismatch` means the window or screenshot size changed.
- **Computed-style differences** — `design/theme/state/surface/property` with
  the baseline and current value. These point at the selector that changed (for
  example `#nav-bar` turning transparent under Lepton) even when the pixel
  change is small.
- **All images** — every comparison, including unchanged ones.

A Runtime bump is expected to move some pixels (font rendering, icon updates).
Look at the diff PNGs: changes confined to the URL bar text or an icon are
usually upstream; whole toolbars changing colour, see-through toolboxes in
`fullscreen`, or blank tiles in `newtab` are the kind of regression this tool is
for. The per-run `manifest.json` also lists per-state notes, such as whether the
context menu opened and the `userChrome.fullscreen.overlap` value.

## Files

- `mod.ts` — CLI (`run`, `compare`)
- `capture.ts` — browser launch, Marionette driving, fixture proxy
- `compare.ts` — pixel and value comparison, Markdown report
- `png.ts` — dependency-free PNG decode/encode
- `fixtures/` — static page and the two local test themes
- `compare.test.ts` — `deno task test:design-matrix`
