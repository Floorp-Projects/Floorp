# Arabic157 memory investigation — existing evidence only

Seven resource curves, fourteen startup logs and seven KAN29 QA profiles were inspected. Only locale/sidebar/design/session-restore allowlisted preferences, locale-addon metadata and hashes were retained. Session contents/URLs and raw logs were not copied. JS heap, DOM counts and exact internal locale activation timing were **not measured**.

## First Arabic launch versus reused state

Candidate Wayland30074 and X11 30075 each started with a fresh QA profile, completed LTR measurement and **normally exited0**, installed compatible signed Arabic157 locale (signedState2, appDisabledfalse), then **normally exited0**. Their first subsequent RTL launches (PIDs113790/114791) grew to12.515/12.905GiB known PSS and were guarded. Therefore the overall observation is not confined to profiles already force-stopped. No strict RTL result was obtained before those guards.

Baseline parent30071 likewise completed LTR and signed locale-install with child exit0; the first Arabic startup did not yield strict locale measurement. Its initial attempt lacks resources.jsonl, so no comparable PSS curve or precise growth timeline can be reconstructed. Baseline resume30073 reused that already interrupted Arabic profile (child-9); this is not a fresh-first-RTL control.

Passive-no-session copied previously guarded candidate30075, retained CLI --marionette but made no socket connection/NewSession. It reached12.273GiB PSS at54.66s from monitor baseline; Arabic chrome was visibly painted. This rules out NewSession being required for growth **in that reused-profile setup**, not a strict universal causal exclusion. Horizontal30080 also copied30075 and changed horizontal startup configuration, so is likewise recovery-state confounded; it peaked12.079GiB.

## Process and startup comparisons

At guarded candidate peaks, the main browser process accounts for over99% of known aggregate PSS; children together remain about80MiB. LTR controls and pre-locale stages peak around0.48–0.61GiB. Exact sampled curves, cached-PSS caveats, stage PID mapping and threshold times are recorded in JSON. No JS-heap/DOM attribution follows from main-process PSS alone.

All fourteen logs contain the same general markers: three pref-parser errors, one `Failed to install lang pack on startup`, one browser-first-window-ready AboutNewTab initialization error, and one Marionette listening marker. These occur in successful low-memory LTR runs too, so their mere presence is not specific evidence of the Arabic growth cause. Log digests and marker line positions retained; no speculative fix proposed. Current post-run profiles all show Arabic locale addon active/signed2 where installed, but are not immutable pre-RTL snapshots. Session-file hashes establish current state only; session contents were not decoded.

## Control qualification

Exact parenta629 is the strongest existing comparison: same157.0.1 Runtime/build20261007125927 and matching native executable/libxul, same Floorp branding, only two flex-order source/test changes to candidatef43. Both exhibit an Arabic startup problem, but baseline first-start detailed curve is absent and all strictRTL target trials remain uncollected. Current official Firefox on disk is159.0.1/build20261102144711, not a same157 control. A prospective Firefox157 control needs verified source/build/native and branding/patch qualification; matching157 version alone would not prove identical native Runtime. No new download, execution, source, PR, CI or lock change occurred in this investigation.
