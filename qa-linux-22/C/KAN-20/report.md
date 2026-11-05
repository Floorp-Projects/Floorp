# KAN-20 / GitHub #2919 — Yahoo repeat reload

Classification: **not reproduced in the tested substitute environment; original Ubuntu26.04 Snap/Flatpak conditions remain untested. No fixed/closed claim.**

Source reference: current main `984509d2053be7bae9027133bda7259cd706b208`; official Floorp12.21.1/Gecko159.0.1 archive (application BuildID20261103231153, Runtime20261103153232). Executable SHA256 `4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d`. Firefox comparison is official159.0.1 BuildID20261102144711.

Real environment: Debian13.6, Linux x86_64, Xvfb/X11, fresh isolated profile/HOME/XDG. This is neither Ubuntu26.04 nor Snap/Flatpak. Real content processes first passed a localhost HTML fixture. Product default protections remain enabled (content sandbox level6; cookieBehavior5; no sandbox-disabling environment values, no TLS acceptance override). Environment's public proxy CA was imported into NSS and the configured proxy used. Marionette exposes navigator.webdriver=true; QA system-access switch permits browser inspection.

Floorp ordinary and native private windows both loaded https://yahoo.com →301→ https://www.yahoo.com →200. Each stayed document.readyState=complete at all six samples across30seconds, with actual Yahoo page content and no repeated apex/www-root requests. Screenshots and sanitized request/status timelines are in the case subdirectories. This observation does not establish that the original issue is fixed or that cookies/service workers caused it. No product defect was established and no code fix or PR was created.

`matrix.json` lists all completed comparison cases and their paths. Raw profiles are disposable local working data and must not be included in shared artifacts. Saved results contain no cookies, authorization headers or token values. Request URLs omit queries.

A separate curl probe saw apex301/www429 while actual browser responses were200, illustrating Cloud/client variability; that HTTP rate limit is not a Floorp bug or the reported loop. Initial missing-current-CA attempts were harness infrastructure failures and were corrected without bypassing verification.

Remaining: original Ubuntu26.04 Snap/Flatpak, workspace/container variants, and a longer observation window if needed. Existing issue-number and keyword PR searches did not identify a specific fix; absence from bounded search is not proof no relevant PR exists. Jira state remains unchanged; generic needs-linux can now be narrowed to the original packaging/OS condition rather than declaring success.

Reproduce with `web-tools/run_web.py` from the parent evidence directory using its isolated venv; run with the managed execution permissions necessary for network, namespaces and browser processes. The default restricted command context caused uid_map/network errors; the authorized unrestricted execution path resolved these and is the basis of the result.

Final comparison: Firefox ordinary Yahoo also loaded200 and remained stable for30seconds. Firefox private Yahoo instead received HTTP503 with an Envoy `cloudflare_https_tunnel` upstream transport-error body. That case is **infrastructure-failed**, not “Yahoo loop unreproduced,” and is marked as such in the machine-readable cases. Three Yahoo cases reached the real page; one comparison case did not.
