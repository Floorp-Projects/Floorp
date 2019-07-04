[android-components](../../index.md) / [mozilla.components.feature.intent](../index.md) / [IntentProcessor](./index.md)

# IntentProcessor

`class IntentProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/intent/src/main/java/mozilla/components/feature/intent/IntentProcessor.kt#L34)

Processor for intents which should trigger session-related actions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `IntentProcessor(sessionUseCases: `[`SessionUseCases`](../../mozilla.components.feature.session/-session-use-cases/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, searchUseCases: `[`SearchUseCases`](../../mozilla.components.feature.search/-search-use-cases/index.md)`, context: <ERROR CLASS>, useDefaultHandlers: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, openNewTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, isPrivate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Processor for intents which should trigger session-related actions. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given intent by invoking the registered handler. |
| [registerHandler](register-handler.md) | `fun registerHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, handler: `[`IntentHandler`](../-intent-handler.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given handler to be invoked for intents with the given action. If a handler is already present it will be overwritten. |
| [unregisterHandler](unregister-handler.md) | `fun unregisterHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the registered handler for the given intent action, if present. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTIVE_SESSION_ID](-a-c-t-i-v-e_-s-e-s-s-i-o-n_-i-d.md) | `const val ACTIVE_SESSION_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
