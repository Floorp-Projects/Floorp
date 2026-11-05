Current Drafts: [#3031](https://github.com/Floorp-Projects/Floorp/pull/3031) and [#3034](https://github.com/Floorp-Projects/Floorp/pull/3034).

# KAN-98 / GitHub #2244: returning gestures rejected before recognition

Source baseline: `984509d2053be7bae9027133bda7259cd706b208`.
Fix: `8d00c6bcca47a533399a06d9a1dd46dad647fa98` (2 files).

## Environment and classification

**Actual reproduction of a specific controller defect.** Debian 13.6 x86_64, Xvfb `:92` (1440x1100, browser window 1280 wide), DBus session, official Floorp 12.21.1 / Gecko 159.0.1 / BuildID 20261103153232, isolated new profiles. W3C pointer actions, not a physical mouse. Browser sandbox/security validation were not disabled. Rootless user namespaces work outside the tool's restrictive execution layer; browser tests use the authorized execution path. This does not establish Arch/Sway/Wayland behavior.

Official tarball SHA256 `7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613`, matching release hashes.txt. Relationship of each binary byte to the source SHA is not independently proven.

## Actual input result

Every configured gesture maps to New Tab. A single up movement creates one tab. Up-down (two long moves), densely sampled up-down (10px increments), and down-up show the New Tab action label while held but create no tab on release in the unmodified official product. Captured events are trusted W3C-generated events; they are not physical input. See `../gesture-29301.json`.

`controller.ts::getTotalMovement()` calculates only start-to-end displacement. When an intentional returning gesture ends at its starting point, `handleMouseUp` treats it as below the activation threshold and resets it before calling the recognizer. The same gate incorrectly permits a native context menu before release. The recognizer itself already supports up-down/down-up.

## Fix and causal checks

The activation gate now measures maximum excursion from the start. A returning gesture stays above the threshold, while repeated movement entirely within the threshold still retains an ordinary right click. Summing the trail was deliberately avoided because tiny repeated jitter could otherwise activate a gesture.

- Official Floorp chrome execution of the existing controller test module, including two new cases: **40 cases passed** with the revised controller (`../source-29312.json`).
- Identical tests against the baseline controller: the returning-gesture test fails before release; the other 39 cases pass (`../source-29313.json`).
- A dedicated copy of the official product with only the corresponding `getTotalMovement` function changed in `omni.ja` successfully creates one tab for each of up, up-down, dense up-down, and down-up (`../gesture-29314.json`). The original product is untouched. This is a diagnostic copy, not a production build of the commit. `product-patch.json` records the exact changed asset hashes.
- `deno lint`, `deno fmt --check` for both changed files and `git diff --check` pass. Full repository suite and full production build were not run.

The browser source-test bundle uses the real controller, configuration, recognizer, display, and Solid runtime. It replaces the HMR root with a normal Solid root and supplies no-op built-in action registrations; the existing test module replaces these registrations with tracked callbacks. This verifies controller behavior in Gecko, not the complete production startup/integration pipeline. The W3C product-copy comparison independently exercises actual action dispatch and rendering.

## Remaining conditions

The separate sparse-input recognizer defect is now addressed in independent Draft #3034 (e6f2dbe7). See sparse-fix-report.md: 63 recognizer cases pass, baseline fails the two new cases, and deterministic sparse W3C product comparisons fail 6/6 before and pass 6/6 after. Neither PR closes KAN-98. Physical input, Arch/Sway/Wayland, varying DPI, full production builds and the original environment remain open. PR3031 full Ubuntu CI passed 281 modules/0 failed plus16 Firefox tasks; its artifact digest and contents were verified.

## Reproduction files

- `../gesture-run.py` + `../gesture-probe.py`: W3C input/label/screenshots/events. `PYTHONPATH=../python python3 ../gesture-run.py floorp 29311 gesture` from this directory (paths in scripts resolve to browser-ui).
- `source-test/bundle.cjs`: bundles the actual controller test module. Requires cached esbuild and Solid paths; adapt those local dependency paths on another machine.
- `../source-run.py` + `../source-probe.py`: run the source test bundle in an actual Floorp chrome session.
- `source-test/original-controller.ts`: original pinned controller snapshot. Recreate from baseline SHA, not the current branch HEAD.
- `../source-29312.json`, `../source-29313.json`: exact browser test results; `../gesture-29301.json`, `../gesture-29314.json`: W3C observations.

Existing merged #2212/#2642/#2763 address other gesture behavior and do not remove this displacement gate in the pinned main source.

Jira追記comment ID: `10291`。needs-linuxは実試験によりlinux-testedへ置換し、原ディストリ/物理入力の具体条件とenv-blockedは保持。

## Right-down follow-up

See `rightdown-followup.md`: an additional fresh-profile comparison succeeded for right-down at sparse100ms, sparse800ms, and dense10px input (each3/3), plus single-up3/3. Replaying earlier failure and fresh success traces in Gecko identifies sample-sensitive first-direction/sequence recognition: initial sparse jumps skip the 10–50px sample band, producing a diagonal initial direction and null match. Configuration was present. This is outside #3031 and original physical Arch/Sway/Wayland behavior remains unverified.

Jira最終comment ID: `10291`。

PR3034標準CIも281モジュール/0失敗+Firefox16task、artifact digest/内容確認済み。
