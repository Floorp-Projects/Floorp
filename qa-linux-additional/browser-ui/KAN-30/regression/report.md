# KAN-30 final QA regression proposal

Final patch sidebarOverlay-regression.patch uses explicit browser window.dispatchEvent, with two narrowly scoped no-window-prefix lint exceptions explaining the event target. Existing withSidebar/setModes/hover helpers are reused. Source baseline29a3eb23145da90706d8a94441373278521cc737. No product fix, new PR or existing PR head change.

Final actual-browser pair: **29935 baseline fails** at internal blur hover retention; **29936 existing-PR2934-equivalent candidate passes** blur retention, deactivate closure, fresh-hover reopening and pointer-leave closure. Both verified own GreD and actual resource/module hashes. Testcopies preserve product implementation; only external chrome test module added. CSP/security unchanged. Native executables copied, not symlinked to original resource roots.

These are MouseEvent/FocusEvent/Event fixtures, not OS XTest or physical input. Parent's actual OS XTest29904/29907 evidence remains separate. QA-only runtime config adapter writes normal prefs because shipped configs.js exports getter but not setter; integrated patch contains original source helpers with no adapter.

Correction: earlier claim that window.dispatchEvent and globalThis.dispatchEvent were equivalent in this execution context was not supported. 29933/29934 globalThis attempt passed on both baseline and candidate, so it is invalid as discriminating regression evidence and is superseded. Final explicit-window29935/29936 restores the required baseline-fail/candidate-pass result. Do not publish the globalThis attempt as successful validation. The cause of its target/timing difference was not further investigated.

Lint-final.log passes; git diff --check passes. Earlier alternate no-node-modules typecheck completed exit0 before the event-target correction; no additional broad check/install/native build was run. It does not prove full official test-suite integration. Host memory pressure caused earlier tool delays; final pair ran sequentially. Runner cleanup removes private display117 and browser process groups; final-summary.json records absent display socket.

Publish patch, scripts, final-summary.json, result-29935.json/result-29936.json, lint-final.log and selected logs. Exclude testcopy binaries, runtime profiles and full source copies. Parent owns Jira/matrix and existing PR2934 comments. No Done/merge/release.
