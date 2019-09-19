[android-components](../../index.md) / [mozilla.components.feature.intent.processing](../index.md) / [TabIntentProcessor](./index.md)

# TabIntentProcessor

`class TabIntentProcessor : `[`IntentProcessor`](../-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/intent/src/main/java/mozilla/components/feature/intent/processing/TabIntentProcessor.kt#L31)

Processor for intents which should trigger session-related actions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabIntentProcessor(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, loadUrlUseCase: `[`DefaultLoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-default-load-url-use-case/index.md)`, newTabSearchUseCase: `[`NewTabSearchUseCase`](../../mozilla.components.feature.search/-search-use-cases/-new-tab-search-use-case/index.md)`, openNewTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Processor for intents which should trigger session-related actions. |

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `fun matches(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this intent processor will handle the intent. |
| [process](process.md) | `suspend fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given intent by invoking the registered handler. |
