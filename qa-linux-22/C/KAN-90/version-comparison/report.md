# KAN-90: published Floorp12.17.2 versus12.21.1, real Thunderbird portal route

## Result

Two actual trials passed: one each for the public official Linux x86_64 Floorp12.17.2 and12.21.1 archives. The same official signed Thunderbird159.0 Flatpak package (`org.mozilla.thunderbird`, commit e3be32008d26a83a5da2e77783688b79a1306bfd71f5c2aa8873922659fd35ec, runtime25.08) clicked a visible local .eml hyperlink. Both produced an observed real portal OpenURI from the Thunderbird sandbox proxy, matching Response0, HTTP GET, and the same responsive Floorp parent and forkServer processes after navigation. No SIGSEGV marker was observed in either owned trial log. No defect was isolated, so no speculative fix/PR was made.

| Floorp release | Runtime version / runtime BuildID | Parent before→after | forkServer before→after | Portal sender→observed app-id | GET |
|---|---|---|---|---|---|
|12.17.2|155.0 /20260902101625|47027→47027|47179→47179|:1.26→org.mozilla.thunderbird|yes|
|12.21.1|159.0.1 /20261103153232|46279→46279|46438→46438|:1.25→org.mozilla.thunderbird|yes|

Floorp content sandbox6 was measured before each click; the harness writes no sandbox preference. Thunderbird's unchanged shipped content sandbox5 and portal defaults mime-handler2/open-uri2 were measured. GTK_USE_PORTAL and MOZ_APP_REMOTINGNAME overrides were empty inside Thunderbird. No test code calls OpenURI: gdbus only reads sender PID attribution. Synthetic X11 pointer input is explicit; no physical input is claimed. Only localhost fixtures were opened; no mail account or credentials were connected.

## Artifact provenance and build identity

The old release is public, draft=false, published2026-09-03T11:58:08Z, asset542589720. The actual archive SHA256 is `2f59e332766d8ca5e966cc655450a0f1d3cf6e286f9f52172f8bd3caa7858ffc`, matching GitHub's declared digest and released hash file. Source release metadata identifies Floorp `38bde3075f197b9a88490875d1534bb79d6159a5`, Runtime `41fb7de3adb7f33223ae5b24b969a93efa29a9aa`. This is a retrieved original product, not a local reconstruction.

The old outer application.ini/platform.ini say20260903040334, while release linux-meta.json and actual Services.appinfo runtime readback say20260902101625. The outer application.ini explicitly says it is unused. Both observations are preserved; symbol matching must use the actually crashed module's correct build identity, not substitute an outer package timestamp. No crash occurred here, so no symbol investigation was needed.

The retained current archive SHA256 `7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613` matches the public12.21.1 release digest. The verifier separately reads its archive members and confirms the tested executable and libxul bytes match. Current source reference remains984509d2053be7bae9027133bda7259cd706b208. Outer current package BuildID20261103231153 is also distinct from actual runtime20261103153232.

A bounded historical Flathub investigation found manifest46747f209ee88905036ecc9f8933583e975ceaa2, which pins the identical12.17.2 archive and runtime25.08. Normal signed remote history exposed only6 commits, oldest Sept30; a historical TEST commit identified by public build logs returned404. A historical signed stable12.17.2 OSTree deployment was **not acquired**. This does not establish global unavailability. See acquisition/report.md, summary.json and small build evidence. Therefore this comparison uses **archive versus archive**, with the same genuine Thunderbird Flatpak sender; it does not claim old-versus-new Floorp Flatpak equivalence.

## Isolation, environment and remaining condition

The original4GB Flatpak prefix and its profiles were preserved. A separate copied prefix under version-comparison/home and fresh per-version Floorp and Thunderbird profiles were used. Desktop registration changes are confined to that copy. The same private DBus/real GTK portal/Xvfb/Openbox harness and the same sender package/runtime were used sequentially. Floorp archives use the same available host library setup; each retains its own release-shipped binary and libxul. The latest scripts are parameterized from the previously proven real Thunderbird route.

Actual host remains Debian13.6 Cloud Linux, not Fedora hardware. The existing Fedora44 container measured Flatpak1.18.4, native Thunderbird158.0, and protected unshare/bwrap EPERM; the passing host path uses Flatpak1.16.6 and signed Thunderbird159.0 runtime25.08. The existing container image/digest and logs remain unchanged. Namespace failures were not repeated, no Docker/Flatpak/Firefox protection was disabled, and no native Fedora desktop/portal/policy equivalence is claimed. This trial cannot isolate Fedora-specific library, portal, kernel, driver or original package differences.

The remaining original condition is a protected native Fedora44 desktop with the reporter's Floorp Flatpak12.17.2 and Thunderbird/package/runtime combination, plus correct-build crash evidence if the SIGSEGV recurs. The general old-version availability gap is closed for the public archive; the original packaged-condition gap remains. KAN71's existing matched Firefox/Floorp activation failures are unchanged; KAN20/21 and KAN64 were not retested.

## Evidence and reproduction

`verification.json` and `raw/<version>/observations.json` contain allowlisted original numeric/PID/GET/app-id observations plus source-result hashes. The two ~2KB DBus traces contain localhost-only fixture URLs and sender/request identifiers. The original result JSON, logs, screenshots and profiles remain local. No profile, cookie, credential or personal URL is included in the small evidence set.

Run `env.sh dbus-run-session -- /workspace/qa-linux-22/C/web-tools/venv/bin/python run-comparison.py <version> <absolute-official-floorp-binary>` from this directory with permitted local GUI/DBus sockets. Reproduction uses the isolated copied installation; for a new independent trial, create another copy/profile prefix rather than reusing existing evidence directories. `python verify.py` audits the two recorded trials without rerunning products.

Per the latest no-publication instruction, no external branch, Jira comment, merge, release or Done transition was performed. A Jira comment draft is saved locally. Last existing KAN90 comment remains10322.
