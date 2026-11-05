# Minimal integrated regression prepared on exact PR2934 head

Base b7866aaa4905be4d86fb3dff1237d6a70c483f10; local head bfb8e5b583e1d8520e8e894e7480276f0250d0f2; branch test/sidebar-hover-regression-pr2934; worktree /workspace/kan30-regression-source. Clean local worktree. One test file,44 additions, no product code. pr2934-exact-head.patch is the minimal Git diff.

PR author EldinBegano, foreign fork EldinBegano/Floorp, branch fix/firefox-sidebar-hover-tab-switch. maintainerCanModify=true but fork permissions report push=false for authenticated Ryosuke-Asano; upstream push/admin=true. No specifically agreed foreign-branch workflow was found. Nothing pushed; no new PR.

Existing sidebarOverlay.test.ts helper/source bytes are identical between exact PR head and main29a baseline (pr2934-helper-diff.txt empty). Integrated patched file is byte-identical to the QA source copy underpinning final29935 baseline-fail/29936 candidate-pass fixture. This is actual browser execution of synthetic hover/blur/deactivate fixtures with a live product module and QA-only normal-pref config adapter; not a full source build, full suite run, OS XTest or physical-input proof. Integrated patch itself uses original withSidebar/helpers and no adapter. Explicit browser window targets have narrowly scoped lint exceptions. lint and diff-check pass. No additional broad typecheck performed.

For a dependent Draft, upstream cannot directly use another fork's branch as its base. Parent may create an owned dependency-base mirror pointing exactly at canonical2934 head and an owned test branch at bfb8e5b5, then create a Draft against that mirror with the44-line test diff. State dependency on canonical2934; do not imply the mirrored product fix is a second canonical fix. Retarget/rebase after canonical integration only under subsequent authorization. Do not write the author's branch.

A patch was transiently applied to the shared checkout because one apply command retained the prior working directory; it was immediately reversed using the exact patch. Shared target file is confirmed clean; all final changes reside only in the isolated local branch.
