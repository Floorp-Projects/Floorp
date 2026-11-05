# Linux22 stabilization — integration C evidence

Assigned five issues: KAN-20/#2919, KAN-21/#2532, KAN-64/#2568, KAN-71/#2535, KAN-90/#2726. This is the C subset, not a claim to have completed all22 Linux issues.

Source: fetched Floorp latest main `984509d2053be7bae9027133bda7259cd706b208`, Floorp12.21.1/Gecko159.0.1. The saved checkout was older; a clean independent worktree/branch was created. Existing Jira descriptions/comments, original GitHub issues/comments and bounded existing-PR searches were checked. No directly established canonical fix PR was found for these five. Bounded search absence is not proof of absence.

Start with `matrix.json`, then each `KAN-*/report.md`. Classification is per issue and per case. Infrastructure-failed navigation is not a product failure or an unreproduced product case. Substitute Debian/Xvfb tests do not establish original Ubuntu/Snap, Zorin, Fedora44/KDE, Linux Mint/Cinnamon or physical-input behavior. Actual Fedora44 container capability failures are recorded separately.

All five assigned issues received real-product tests or integration execution. KAN-64 symptom is reproducible, with matching TST/Firefox hidden-member behavior identifying an extension/upstream integration dependency. No Floorp-only code defect has been established; no speculative fix PR was created. Original-condition gaps remain explicit. No merge, release or Done transition occurred.

Portable export contains selected reproduction scripts, reports, sanitized machine-readable observations, public API contract excerpts, screenshots and hashes. It omits downloaded applications/dependencies/XPI, disposable HOME/profiles, cookies/NSS stores, credentials, full process environment, and unrelated work. Reproduction scripts state their original `/workspace/qa-linux-22/C` dependency paths; recreate that layout with the recorded official product versions and dependencies or adapt only those paths. Use the authorized execution context permitting local sockets/user namespaces; restricted-command setup failures are separately recorded in environment/probe.json.

No product sandbox, TLS verification or challenge verification was disabled to obtain a passing result. The Turnstile demo's always-pass public **test key** is explicitly a rendering fixture, not authentication success. Real-site login tests entered no credentials and submitted no login. Product inspection requires Marionette's documented system-access opt-in.

The `qa/linux-c-integration` branch exists solely for retrieval/review of QA evidence. It is not a release branch or a proposed product fix. Reports retain reproduction boundaries and remaining conditions for the coordinating session.

Execution count: five assigned issues received actual-product tests. Web comparisons:16 attempted cases,9 reached their target/fixture and7 were infrastructure-failed; the rendering-only Turnstile fixture does not count as real authentication. Focus comparison:8 URL dispatches reached the fixture,0 raises in both products. Thunderbird:2 real local-mail-link Flatpak remoting controls succeeded; full portal dispatch and original Fedora environment remain unvalidated.

Portable branch scope: only reviewed authored reports/scripts and bounded summary JSON. Screenshots, raw logs, third-party HTML/API source, complete result files and all profiles remain local at /workspace/qa-linux-22/C and are NOT in this branch. References to those files in reports describe local evidence. No private browser content, credential values, user metadata or opaque challenge tokens are included.
