[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabIntentProcessor](./index.md)

# CustomTabIntentProcessor

`class CustomTabIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabIntentProcessor.kt#L22)

Processor for intents which trigger actions related to custom tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, resources: <ERROR CLASS>)`<br>Processor for intents which trigger actions related to custom tabs. |

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `fun matches(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this intent processor will handle the intent. |
| [process](process.md) | `suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |
