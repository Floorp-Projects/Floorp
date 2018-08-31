---
title: SessionIntentProcessor - 
---

[mozilla.components.feature.session](../index.html) / [SessionIntentProcessor](./index.html)

# SessionIntentProcessor

`class SessionIntentProcessor`

Processor for intents which should trigger session-related actions.

### Parameters

`openNewTab` - Whether a processed Intent should open a new tab or open URLs in the currently
    selected tab.

### Constructors

| [&lt;init&gt;](-init-.html) | `SessionIntentProcessor(sessionUseCases: `[`SessionUseCases`](../-session-use-cases/index.html)`, sessionManager: SessionManager, useDefaultHandlers: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, openNewTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Processor for intents which should trigger session-related actions. |

### Functions

| [process](process.html) | `fun process(intent: Intent): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes the given intent by invoking the registered handler. |
| [registerHandler](register-handler.html) | `fun registerHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, handler: `[`IntentHandler`](../-intent-handler.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given handler to be invoked for intents with the given action. If a handler is already present it will be overwritten. |
| [unregisterHandler](unregister-handler.html) | `fun unregisterHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the registered handler for the given intent action, if present. |

### Companion Object Properties

| [ACTIVE_SESSION_ID](-a-c-t-i-v-e_-s-e-s-s-i-o-n_-i-d.html) | `const val ACTIVE_SESSION_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

