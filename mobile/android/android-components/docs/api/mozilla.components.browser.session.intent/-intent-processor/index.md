[android-components](../../index.md) / [mozilla.components.browser.session.intent](../index.md) / [IntentProcessor](./index.md)

# IntentProcessor

`interface IntentProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/intent/IntentProcessor.kt#L12)

Processor for Android intents which should trigger session-related actions.

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `abstract fun matches(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this intent processor will handle the intent. |
| [process](process.md) | `abstract suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabIntentProcessor](../../mozilla.components.feature.customtabs/-custom-tab-intent-processor/index.md) | `class CustomTabIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which trigger actions related to custom tabs. |
| [TabIntentProcessor](../../mozilla.components.feature.intent/-tab-intent-processor/index.md) | `class TabIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which should trigger session-related actions. |
| [WebAppIntentProcessor](../../mozilla.components.feature.pwa.intent/-web-app-intent-processor/index.md) | `class WebAppIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which trigger actions related to web apps. |
