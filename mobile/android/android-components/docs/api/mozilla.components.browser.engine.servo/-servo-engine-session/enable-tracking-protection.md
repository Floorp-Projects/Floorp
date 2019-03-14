[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](index.md) / [enableTrackingProtection](./enable-tracking-protection.md)

# enableTrackingProtection

`fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L114)

Overrides [EngineSession.enableTrackingProtection](../../mozilla.components.concept.engine/-engine-session/enable-tracking-protection.md)

Enables tracking protection for this engine session.

### Parameters

`policy` - the tracking protection policy to use, defaults to blocking all trackers.