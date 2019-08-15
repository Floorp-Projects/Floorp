[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](index.md) / [enableTrackingProtection](./enable-tracking-protection.md)

# enableTrackingProtection

`abstract fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](-tracking-protection-policy/index.md)` = TrackingProtectionPolicy.strict()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L412)

Enables tracking protection for this engine session.

### Parameters

`policy` - the tracking protection policy to use, defaults to blocking all trackers.