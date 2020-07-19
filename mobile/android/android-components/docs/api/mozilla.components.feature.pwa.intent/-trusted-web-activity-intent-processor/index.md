[android-components](../../index.md) / [mozilla.components.feature.pwa.intent](../index.md) / [TrustedWebActivityIntentProcessor](./index.md)

# TrustedWebActivityIntentProcessor

`class TrustedWebActivityIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/intent/TrustedWebActivityIntentProcessor.kt#L39)

Processor for intents which open Trusted Web Activities.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrustedWebActivityIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, packageManager: <ERROR CLASS>, relationChecker: `[`RelationChecker`](../../mozilla.components.service.digitalassetlinks/-relation-checker/index.md)`, store: `[`CustomTabsServiceStore`](../../mozilla.components.feature.customtabs.store/-custom-tabs-service-store/index.md)`)`<br>Processor for intents which open Trusted Web Activities. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
