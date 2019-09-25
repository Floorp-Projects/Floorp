[android-components](../../index.md) / [mozilla.components.feature.pwa.intent](../index.md) / [WebAppIntentProcessor](index.md) / [process](./process.md)

# process

`suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/intent/WebAppIntentProcessor.kt#L42)

Overrides [IntentProcessor.process](../../mozilla.components.feature.intent.processing/-intent-processor/process.md)

Processes the given [Intent](#) by creating a [Session](../../mozilla.components.browser.session/-session/index.md) with a corresponding web app manifest.

A custom tab config is also set so a custom tab toolbar can be shown when the user leaves
the scope defined in the manifest.

