[android-components](../../index.md) / [mozilla.components.feature.intent.processing](../index.md) / [IntentProcessor](./index.md)

# IntentProcessor

`interface IntentProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/intent/src/main/java/mozilla/components/feature/intent/processing/IntentProcessor.kt#L12)

Processor for Android intents which should trigger session-related actions.

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `abstract fun matches(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this intent processor will handle the intent. |
| [process](process.md) | `abstract suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabIntentProcessor](../../mozilla.components.feature.customtabs/-custom-tab-intent-processor/index.md) | `class CustomTabIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which trigger actions related to custom tabs. |
| [TabIntentProcessor](../-tab-intent-processor/index.md) | `class TabIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which should trigger session-related actions. |
| [TrustedWebActivityIntentProcessor](../../mozilla.components.feature.pwa.intent/-trusted-web-activity-intent-processor/index.md) | `class TrustedWebActivityIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which open Trusted Web Activities. |
| [WebAppIntentProcessor](../../mozilla.components.feature.pwa.intent/-web-app-intent-processor/index.md) | `class WebAppIntentProcessor : `[`IntentProcessor`](./index.md)<br>Processor for intents which trigger actions related to web apps. |
