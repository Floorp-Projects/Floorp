---
title: SystemEngineSession.enableTrackingProtection - 
---

[mozilla.components.browser.engine.system](../index.html) / [SystemEngineSession](index.html) / [enableTrackingProtection](./enable-tracking-protection.html)

# enableTrackingProtection

`fun enableTrackingProtection(policy: TrackingProtectionPolicy): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

See [EngineSession.enableTrackingProtection](#)

Note that specifying tracking protection policies at run-time is
not supported by [SystemEngine](../-system-engine/index.html). Tracking protection is always active
for all URLs provided in domain_blacklist.json and domain_overrides.json,
which both support specifying categories. See [UrlMatcher](#) for how to
enable/disable specific categories.

