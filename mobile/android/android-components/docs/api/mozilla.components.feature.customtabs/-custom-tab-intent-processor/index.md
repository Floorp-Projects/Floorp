[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabIntentProcessor](./index.md)

# CustomTabIntentProcessor

`class CustomTabIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabIntentProcessor.kt#L25)

Processor for intents which trigger actions related to custom tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, resources: <ERROR CLASS>, isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Processor for intents which trigger actions related to custom tabs. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
