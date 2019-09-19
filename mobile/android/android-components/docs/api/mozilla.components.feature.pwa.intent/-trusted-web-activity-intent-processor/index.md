[android-components](../../index.md) / [mozilla.components.feature.pwa.intent](../index.md) / [TrustedWebActivityIntentProcessor](./index.md)

# TrustedWebActivityIntentProcessor

`class TrustedWebActivityIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/intent/TrustedWebActivityIntentProcessor.kt#L37)

Processor for intents which open Trusted Web Activities.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrustedWebActivityIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, packageManager: <ERROR CLASS>, apiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, store: `[`CustomTabsServiceStore`](../../mozilla.components.feature.customtabs.store/-custom-tabs-service-store/index.md)`)`<br>Processor for intents which open Trusted Web Activities. |

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `fun matches(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this intent processor will handle the intent. |
| [process](process.md) | `suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given [Intent](#). |
