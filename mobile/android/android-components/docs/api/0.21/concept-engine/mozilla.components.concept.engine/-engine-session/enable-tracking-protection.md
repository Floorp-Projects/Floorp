---
title: EngineSession.enableTrackingProtection - 
---

[mozilla.components.concept.engine](../index.html) / [EngineSession](index.html) / [enableTrackingProtection](./enable-tracking-protection.html)

# enableTrackingProtection

`abstract fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](-tracking-protection-policy/index.html)` = TrackingProtectionPolicy.all()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Enables tracking protection for this engine session.

### Parameters

`policy` - the tracking protection policy to use, defaults to blocking all trackers.