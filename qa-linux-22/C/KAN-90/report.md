# KAN-90 / GitHub #2726

## Result

**Original Fedora44/Flatpak/Thunderbird forkserver SIGSEGV: environment-insufficient; still unvalidated.** Two actual Debian Thunderbird→Flatpak Floorp local-mail-link controls succeeded. They do not reproduce or disprove the original crash. No product code defect was established, and no duplicate/fix PR was created.

Source main: `984509d2053be7bae9027133bda7259cd706b208` (parent verified latest). Flathub signed Floorp12.21.1 / Gecko159.0.1, app commit `ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621`, executable SHA256 `4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d`.

## Actual tests

- Created Fedora44 container from official image digest `sha256:15da97ed9aa94dd3d92c3925eb89520f6c80e07ea7ee24cba848d1f8b6912b3e`. Installed Flatpak1.18.4, Thunderbird158.0, DBus, portal, Xvfb and Openbox. HTTPS repository access passed using the worker CA bundle. Nonroot `unshare -Ur true` and `bwrap --ro-bind / / --unshare-user -- true` both failed EPERM with default Docker protection. No seccomp, user-namespace protection or product sandbox was disabled. Thus this container cannot supply the original protected Flatpak execution environment. This is an infrastructure capability failure, not Floorp crash evidence.
- Built a user-space Debian13.6 fixture with Flatpak1.16.6, Thunderbird140.12.0esr, Xvfb1280×900, Openbox, private session DBus, GTK portal and signed Flathub Floorp. Fresh dedicated HOME/profile directories were created; later controls reused these QA-only profiles. No personal mailbox or existing user profile was connected.
- Opened `fixture.eml` in actual Thunderbird's standalone message window. The HTML link points only to `http://127.0.0.1:19090/from-thunderbird`. A synthetic X11 pointer click at visible link coordinates (200,215) caused an HTTP GET and added the marker tab in the already-running Flatpak Floorp session. The same live Marionette session returned parent PID2 before/after (PID is within the Flatpak namespace), and the existing browser remained responsive. No SIGSEGV was observed in the owned trial logs; forkserver survival and the effective numeric content-sandbox preference were not independently snapshotted. No numeric sandbox-level assertion is made. Two such successful controls were recorded; an earlier click at wrong coordinates produced no GET and is not counted.
- Tried `GTK_USE_PORTAL=1`, then the additional Thunderbird `widget.use-xdg-desktop-portal.open-uri=2` preference. Session monitoring recorded `org.freedesktop.portal.OpenURI.SchemeSupported` calls, **but no `OpenURI` method call**. Therefore the observed successful path is Thunderbird→desktop/GIO→Flatpak remoting, and full portal URI dispatch remains unvalidated. Preference selection alone is not portal-execution evidence.
- Ordinary protection defaults were retained. No `MOZ_DISABLE_CONTENT_SANDBOX`, no no-session-bus/no-a11y switches, no certificate or signature-check disabling. The cached worker env.sh that disables content sandbox was not sourced. Marionette was enabled for observation with TLS validation retained (`acceptInsecureCerts=False`); the explicit remote-system-access switch permits local test inspection only.

## Fixture limits

This is Debian plus Xvfb/Openbox, not Fedora's native desktop, KDE, WSLg, a physical input test, or a desktop login session. It has no host system bus. A private DBus address was also supplied as the fixture's system-bus address; libmalcontent's ordinary lookup reported parental controls globally disabled in this fixture. No policy was edited or disabled by the test. That lookup does not validate a real Fedora AccountsService or policy environment. GTK portal exported OpenURI but did not receive actual URI dispatch. Missing RealtimeKit/PipeWire services were logged separately; they are not the reported forkserver SIGSEGV.

## Remaining conditions / next test

A Fedora44 desktop able to run Flatpak/bubblewrap under its normal protection, Thunderbird and the original/current app runtime, with real session/system buses and actual portal OpenURI invocation. Compare Firefox Flatpak under identical conditions, retain matching-build symbolized coredumps if SIGSEGV recurs, and test reported12.17.2 if available. The original unsymbolized libxul+0x53b166e report alone cannot identify a Floorp overlay defect. No merge, release or Done transition.

## Evidence / reproduction

`reproduce.sh`, `flatpak-env.sh`, `run-integration.py`, `fixture.eml`, `integration-result.json`, `dbus-monitor.log`, `before-link.png`, `after-link.png`; first successful comparison under `direct-control/`. `fedora-probe.log`, `fedora-capability.log`, `docker-pull.log`, `product-info.txt`. Bulky dependencies, profiles, cookies, private cert/key stores and generated databases are excluded from the export allowlist.
