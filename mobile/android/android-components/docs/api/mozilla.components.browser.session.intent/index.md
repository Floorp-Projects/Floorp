[android-components](../index.md) / [mozilla.components.browser.session.intent](./index.md)

## Package mozilla.components.browser.session.intent

### Types

| Name | Summary |
|---|---|
| [IntentProcessor](-intent-processor/index.md) | `interface IntentProcessor`<br>Processor for Android intents which should trigger session-related actions. |

### Properties

| Name | Summary |
|---|---|
| [EXTRA_SESSION_ID](-e-x-t-r-a_-s-e-s-s-i-o-n_-i-d.md) | `const val EXTRA_SESSION_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [getSessionId](get-session-id.md) | `fun <ERROR CLASS>.getSessionId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>`fun `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`.getSessionId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Retrieves [mozilla.components.browser.session.Session](../mozilla.components.browser.session/-session/index.md) ID from the intent. |
| [putSessionId](put-session-id.md) | `fun <ERROR CLASS>.putSessionId(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): <ERROR CLASS>`<br>Add [mozilla.components.browser.session.Session](../mozilla.components.browser.session/-session/index.md) ID to the intent. |
