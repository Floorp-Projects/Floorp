[android-components](../../index.md) / [mozilla.components.feature.pwa.intent](../index.md) / [WebAppIntentProcessor](./index.md)

# WebAppIntentProcessor

`class WebAppIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/intent/WebAppIntentProcessor.kt#L28)

Processor for intents which trigger actions related to web apps.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, storage: `[`ManifestStorage`](../../mozilla.components.feature.pwa/-manifest-storage/index.md)`)`<br>Processor for intents which trigger actions related to web apps. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#) by creating a [Session](../../mozilla.components.browser.session/-session/index.md) with a corresponding web app manifest. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTION_VIEW_PWA](-a-c-t-i-o-n_-v-i-e-w_-p-w-a.md) | `const val ACTION_VIEW_PWA: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
