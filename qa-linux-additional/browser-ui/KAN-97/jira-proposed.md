## Linux actual verification and narrowly scoped candidate

Official12.21.1/Gecko159.0.1 and existing full strict3031+3034 candidate were tested with owned Xvfb/Openbox, normal focus and actual synthetic OS gesture input on controlled HTTP documents. Root and focused overflow top/bottom gestures work (4/4 per product). Thus the original Windows/HuggingFace root-page condition is not reproduced by these controls and remains unverified.

A distinct defect is reproduced: when same-origin or cross-origin iframe content is focused, the correct label appears but all4 frame gesture cases scroll the unrelated top document; the frame remains1800px. Native Ctrl+Home/End works on the same focused frames in Floorp and Firefox.

Draft https://github.com/Floorp-Projects/Floorp/pull/3047 limits routing to the focused content context only when its top belongs to the selected tab, otherwise falling back to that selected tab. Source d6bd2fc457774f55ab1f36a2b01c941f908f1ab0. Diagnostic compiled-module candidate:8/8 gesture endpoints and8/8 keyboard endpoints pass; non-target positions unchanged, menu0, exactly8 dispatches. Browser policy tests7/7 pass; old-route adapter2 assertion failures is secondary evidence, actual baseline OS failures are primary. This is not a full source-built package and does not establish the original report's cause. No protection or actor whitelist was disabled; canonical products unchanged.

Proposed labels: retain needs-repro for original report, add linux-tested and focused-frame-reproduced; remaining condition: original-windows-page-focus. No status/Done transition. Report and matrix are in qa-linux-additional/browser-ui/KAN-97, separate from Linux22.
