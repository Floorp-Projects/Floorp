# KAN-87 / GitHub #2691: actual panel body + Zen paint on nested Wayland

**Result: not reproduced in six tested paint conditions.** Existing merged PR #2767 remains the implementation reference. No product code was changed and no duplicate PR was created. Original CachyOS/Niri/Hyprland/MangoWM conditions are still unfulfilled.

## Environment and product

Debian13.6 x86_64, KWin6.3.6 nested on Xvfb :92. This run owns a private DBus session, `qa-kde-zen` Wayland socket and `runtime/wayland-zen`; it does not share another assignment's KWin socket. Final run port29655. Official Floorp12.21.1 / Gecko159.0.1 / BuildID20261103153232. `Troubleshoot` reports **windowProtocol=wayland**, WebRender (Software), llvmpipe, safeMode=false and default-enabled Fission. This is actual KWin/Wayland client execution, not merely an Xvfb browser run; it is not a full KDE/CachyOS desktop or a physical GPU/display.

Source analysis baseline `984509d2053be7bae9027133bda7259cd706b208`. The official Floorp archive hash previously matched its release hashes.txt. Binary/source byte identity is not independently established.

## UI route and fixtures

1. Load a data-URL test page with a solid high-contrast backdrop.
2. W3C pointer-click `#default-panel-bookmarks` in the right rail. This opens the actual Bookmarks panel body, not just the rail. Normal-mode panel width400px, right rail42px.
3. Send W3C Alt+T and pointer-click the real Tools-menu `#toggle_zenmode` item. No Zen controller API, root-attribute manipulation or implementation patch was used.
4. Hover the top-right corner with the W3C pointer. The product itself sets `zenmode-reveal-top` and `zenmode-reveal-side` together. The actual panel body remains400px wide and opacity1, and the toolbox remains1366x73px and opacity1.
5. Capture default theme, color-only theme and image theme, each with red and cyan page backgrounds. The theme pair shares its colors, with only `theme_frame` image presence differing.

W3C keyboard/pointer input is synthetic input, not a physical device. Background color changes are test-page content fixtures. Test themes are unsigned **temporary development themes** installed through the ordinary `AddonManager.installTemporaryAddon` API and enabled normally. Both report type=theme, isActive=true, temporarilyInstalled=true. No signature or sandbox preference was changed; the fresh profile has no user-set `xpinstall.signatures.required`, `extensions.langpacks.signatures.required`, `security.sandbox.content.level` or `security.sandbox.gpu.level`. This does not claim an AMO/signed-theme installation test. `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` enables the documented automation interface; no content-sandbox-disabling variable is used.

The theme manifest requests an rgba frame color; the theme engine resolves the observed computed color to opaque rgb(174,200,240). Therefore these observations are about the actual resolved theme, not proof that arbitrary translucent AMO theme colors are preserved with alpha.

## Actual paint evidence

`29655-paint.json` completes all six conditions without error and records DOM bounds, computed backgrounds, image URL, opacity, Zen state, panel browser URL and Wayland diagnostics. Both simultaneous-reveal attributes and actual body bounds are checked for every capture. Images are under `screenshots/29655-*-simultaneous.png`.

Red/cyan screenshot comparisons in every theme:

| Region | Pixels compared | Changed pixels | Maximum channel delta |
|---|---:|---:|---:|
| Empty tabbar frame | 17,500 | 0 | 0 |
| Frame directly above right-panel body | 2,750 | 0 | 0 |
| Navigation toolbar | 16,800 | 0 | 0 |
| Panel body | 116,000 | 0 | 0 |
| Page backdrop positive control | 250,000 | 250,000 | 255 |

The default frame paints rgb(235,235,237); the color-only fixture paints rgb(174,200,240). The image fixture paints all four expected stripe colors, and five sampled top-frame pixels exactly match the corresponding source PNG pixels after right alignment. The image remains visible above the revealed right panel. These are rendered-browser screenshot pixels, not computed-style inference alone or physical-monitor scanout measurements.

Within the tested regions and states, the toolbar does not acquire the page backdrop or mix with the revealed panel, and theme artwork is retained. `29655-pixels.json` preserves coordinates and color counts; `summary.json` includes the exact source-image pixel checks and security-preference user-value checks.

## Earlier attempts and limits

- 29651 inspected the live native-Wayland DOM and established the real panel controls.
- 29652 captured two valid default-theme paint conditions, then an optional close/reopen sequence lost its Marionette socket.
- 29653 completed six paint conditions using an earlier dark fixture palette; its optional UI reopen completed. The light text on the earlier panel fixture was a theme-fixture color choice, not evidence for KAN-93.
- 29654 captured the six final-palette paint conditions, then the optional close/reopen sequence reported a discarded browsing context. These optional failures do not establish a product crash or a defect cause. They were not counted as paint failures or as successful completion of that post-sequence.
- 29655 omits that unnecessary post-sequence and cleanly completes the six requested paint conditions. This is the final result. Earlier JSON and logs remain separately named for audit.

Original compositor/distribution combinations, physical input and hardware GPU composition remain unverified. This does not close KAN-87 or justify removing labels for the original CachyOS/Niri requirements.

## Reproduction and publication

`dbus-run-session -- bash nested-zen.sh 29655 paint` runs the private compositor/browser setup; adapt the absolute workspace base in `nested-zen.sh` on another machine. `make-theme.py` generates the small PNG/manifest/XPI fixtures. `measure-pixels.py 29655` compares rendered screenshots. Browser and Python dependencies are resolved from the parent browser-ui directory; do not publish runtime profiles or dependency/product directories.

Publish the scripts, this report, `summary.json`, final29655 JSON/pixel JSON/screenshots, `theme/frame.png`, both `theme/*-manifest.json`, and minimal attempt/error logs. The XPI packages can be regenerated from the fixture script without changing security settings.
