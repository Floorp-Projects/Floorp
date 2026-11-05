# KAN-39: interior actual-wheel comparison

Official Linux Floorp 12.21.1 (Gecko 159.0.1, BuildID 20261103153232), Debian 13.6, private Xvfb :98, DPR 1. Fresh profile, 60 about:blank tabs; the same tabs were grouped for the stack condition. OS XTest button4/button5 delivered trusted DOM_DELTA_LINE deltaY -6/+6. These are synthetic OS events, not a physical mouse or Fedora desktop.

The native horizontal arrowscrollbox intentionally ignores its first two same-axis events (`_prevMouseScrolls=[null,null]`, `toolkit/content/widgets/arrowscrollbox.js` on_wheel). Cold single-notch results are preserved separately and are invalid as distance comparisons. The final script sends three real same-axis warmup notches, then repositions each scroller to its exact midpoint and checks it remains stable before each measured notch. It does not alter the axis filter or handlers.

| Product / strip | width / total (px) | start (px) | button4 / button5 displacement (px) |
|---|---:|---:|---:|
| Floorp normal | 951 / 4516 | 1782.5 | -456 / +456 |
| Floorp stack | 1181 / 5092 | 1955.5 | -509.19995 / +509.19995 |
| Firefox normal | 1124 / 4636 | 1756 | -456 / +456 |

Both measured endpoints remain interior. Both events reached the intended strip and were trusted, line-mode, and consumed. Stack distance is 1.1167 times native normal distance for this geometry; current Linux stack slow scrolling is not reproduced in this condition. Different average tab widths explain why equal line input need not give equal pixel distance. This does not resolve original Fedora 44 physical-wheel speed, acceleration, DPI, Wayland or perception conditions.

No code, protection preference, sandbox level, signing requirement or existing PR was modified. Floorp's official product has xpinstall.signatures.required=false as its existing default (userSet=false), recorded rather than asserted true. Sandbox level is 6. The script writes only startup/automation/stack fixture preferences. Existing merged PR 2763 remains canonical; no duplicate PR.

Reproduce: `PYTHONPATH=/workspace/qa-linux-22/browser-ui/python python run.py floorp 29839`, then `python run.py firefox 29840` with the same PYTHONPATH and outer execution permission for Xvfb/browser IPC. Paths to product and installed XTest dependencies are explicit in the script. Logs, raw measurements, cold controls and screenshots are alongside this report. Profiles remain outside publishable QA under /workspace/kan39-followup-run.

Classification: not reproduced under tested synthetic Linux condition; remaining original distribution/physical-device conditions. Jira update is handled by the parent; no comment was posted by this child.

Firefox 159.0.1 BuildID 20261102144711 matches Floorp native normal in both directions after warmup. Its signature requirement remains true (userSet=false), sandbox6. All six final measured cases have stable interior starts and matching actual ±6 line input.
