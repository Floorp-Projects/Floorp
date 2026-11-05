# Native focus and normal Sidebery follow-up, 2026-11-06 JST

PR2934 remains author EldinBegano's fork branch, head b7866aaa4905be4d86fb3dff1237d6a70c483f10. maintainerCanModify=true does not establish an agreed author-branch workflow; the authenticated actor has no ordinary fork push permission. The author branch was not touched. Owned immutable base mirror qa/pr2934-base-b7866aaa points exactly at that head. Dependent Draft3043 adds only44 test lines on top (head bfb8e5b583e1d8520e8e894e7480276f0250d0f2). It is not a second canonical product fix, and neither PR may be merged for this task.

The earlier focus-control limitation was traced to Marionette's RecommendedPreferences setting focusmanager.testmode=true: the documented purpose is to allow background applications to keep focus. It was not a product behavior result. Fresh profiles now explicitly set this preference false before startup, restoring normal native focus behavior; no sandbox, TLS or signing protection was weakened. Openbox3.6.1-12+b2 and official Debian library dependencies were extracted privately. XDG_DATA_DIRS points only the WM at its normal theme assets; no host package installation. _NET_SUPPORTING_WM_CHECK and _NET_ACTIVE_WINDOW prove an EWMH WM is actually running.

## Qualified real-product comparisons

| Case | Official12.21.1 | Diagnostic PR2934 copy |
|---|---|---|
| Ctrl+Tab, hover only |29944: selected tab changes, pointer remains inside, opacity0 |29945: selected tab changes, opacity1 |
| Ctrl+Tab, sidebar focused |29944: opacity0 |29945: opacity1 |
| Pointer leaves |29944: closes to0 |29945: closes to0 |
| Another owned mapped window becomes active |29944: trusted deactivate, closes0 |29945: trusted deactivate, closes0 |
| Normal signed Sidebery toolbar + actual row click |29946: QA A→QA B, opacity0 |29947: QA A→QA B, opacity1 |

Each Sidebery run is a fresh normal AddonManager installation of the signed, non-temporary5.6.1 package. Ordinary toolbar action selects its native sidebar; the row is selected with OS XTest. Neither showInitially nor forced UI attributes/implementation changes are used in29946/29947. Row-label text paint is blank in this Cloud setup, but the painted row's actual click changes the real selected tab and red→cyan content page, independently confirming selection. NativeSidebar isOpen remains true. Screenshots preserve this distinction. The original Windows desktop and physical input remain untested.

The native-control event trace spans creation/mapping of an owned xmessage. Openbox can focus a newly mapped window before the subsequent explicit EWMH activation request. Therefore documentFocusedBefore in29944/29945, collected after child mapping, is already false; it is not a pre-transition sample. The trusted deactivate trace, unchanged stationary pointer, different _NET_ACTIVE_WINDOW, final focus=false and opacity0 qualify real native app deactivation. Probe.py now collects the before-focus value before spawning the other window to remove that diagnostic ambiguity on future product runs. Earlier29942 with focus simulation true remains unqualified.

Actual product copies still carry the precise compiled-listener diagnostic limitation described in report.md. Source/package candidate preparation is separate and must retain strict validation; neither an assembled archive nor a mismatched158/159 hybrid qualifies as a full candidate package. PR3043's normal CI runs once automatically; do not duplicate it without a new failure/change.

Resource handling: shared old package outputs were retained after automatic approval review rejected their deletion, and a shared-inode hardlink build proposal was also rejected. Only stopped dedicated KAN30 disposable profiles and diagnostic regression copies were removed after immutable evidence retention and process checks. Independent read-only Runtime mount/package work proceeds separately; resource failures must remain bounded and recorded.

Portable runner inputs: design-fixture.json contains only public default design settings (no URLs/user data). QA_PRODUCT selects the executable; QA_PYTHONPATH, QA_XVFB and QA_XDOTOOL can point to equivalent installed dependencies; optional QA_EXPECT_GRE verifies the product resource root. download-sidebery.py retrieves the public signed fixture and checks its known SHA/size; no XPI binary is committed. QA_SOURCE_SHA/QA_CANDIDATE_KIND separate full source-built candidates from compiled-listener diagnostic copies.
