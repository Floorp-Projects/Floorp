[android-components](../index.md) / [mozilla.components.browser.engine.gecko.ext](index.md) / [toContentBlockingSetting](./to-content-blocking-setting.md)

# toContentBlockingSetting

`fun `[`TrackingProtectionPolicy`](../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`.toContentBlockingSetting(safeBrowsingPolicy: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SafeBrowsingPolicy`](../mozilla.components.concept.engine/-engine-session/-safe-browsing-policy/index.md)`> = arrayOf(EngineSession.SafeBrowsingPolicy.RECOMMENDED)): `[`Settings`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/ContentBlocking/Settings.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/ext/TrackingProtectionPolicy.kt#L16)

Converts a [TrackingProtectionPolicy](../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md) into a GeckoView setting that can be used with [GeckoRuntimeSettings.Builder](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntimeSettings/Builder.html).

