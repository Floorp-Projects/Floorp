# KAN-90: independent real portal OpenURI control

**Passed on the Debian fixture.** This followup invokes the installed, real `xdg-desktop-portal`/GTK backend directly. It is separate from the earlier actual Thunderbird local-mail clicks, whose bus trace contained only SchemeSupported and did not prove portal URI dispatch. No portal response was mocked.

- Source reference: `984509d2053be7bae9027133bda7259cd706b208`.
- Signed Flathub Floorp12.21.1/Gecko159.0.1, app commit `ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621`; same executable hash recorded in `../product-info.txt`.
- Debian13.6, Xvfb `:192` 1280×900, Openbox, Flatpak1.16.6, real private session DBus and user-extracted xdg-desktop-portal1.20.3/GTK1.15.3. The private session address also supplies a fixture system bus; it is not Fedora's system/desktop login session.
- `gdbus call ... org.freedesktop.portal.OpenURI.OpenURI '' http://127.0.0.1:19092/from-real-portal "{'ask': <false>}"` returned a real request object. The request used the QA desktop's configured default handler. No chooser click or accept response injection was needed.
- `dbus-monitor.log` records actual **OpenURI**, then the matching Request.Response with **uint32 0**. This is stronger than SchemeSupported probing.
- The local server received **GET /from-real-portal** at UTC14:51:26.638. The browser screenshot shows the returned marker page and URL.
- Same live Marionette session: browser parent PID **2→2** within the Flatpak namespace. `flatpak ps` reports the same host instance PID **31151→31151**. The marker tab appeared in that existing browser.
- Read-only `ChromeUtils.requestProcInfo()` snapshots show **forkServer PID111 before and after** the request. The content-sandbox preference was measured **6 before and after**. No sandbox/content protection, DBus proxy, signature or certificate validation was disabled. Marionette permits local test observation; `acceptInsecureCerts=False`.
- No SIGSEGV/crash was observed in the owned trial logs. This is a bounded before/after observation, not an assertion about long-running crash freedom.

The original Fedora44/Thunderbird/Flatpak forkserver crash remains unvalidated. The separate Fedora44 default-protection container probe failed user namespaces/bubblewrap with EPERM. This direct Debian portal control demonstrates that real portal→existing Flatpak Floorp dispatch can work in the available protected fixture; it neither proves Thunderbird actually used the portal nor disproves the original Fedora-specific symptom.

No product defect or fix PR is justified by this control. Remaining: native Fedora44 protected Flatpak desktop, actual Thunderbird-origin OpenURI trace, original12.17.2/current comparison, and matching symbolized crash evidence if it recurs. No merge/release/Done transition.

Run with the prior KAN-90 isolated dependencies:

```
/workspace/qa-linux-22/C/KAN-90/flatpak-env.sh dbus-run-session -- /workspace/qa-linux-22/C/web-tools/venv/bin/python /workspace/qa-linux-22/C/KAN-90/portal-followup/run-portal.py
```

`run-portal.py` creates only local fixture browser/profile data. The export list includes script, concise result, actual bus trace, product/portal logs and screenshots; profile storage/cookies/private key databases are excluded. Owned child processes and private buses were stopped after measurement.
