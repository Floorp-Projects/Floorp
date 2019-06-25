[android-components](../../../index.md) / [mozilla.components.browser.session.action](../../index.md) / [SessionListAction](../index.md) / [RemoveSessionAction](./index.md)

# RemoveSessionAction

`data class RemoveSessionAction : `[`SessionListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L34)

Removes the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md) from the list of sessions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveSessionAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Removes the [SessionState](../../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](session-id.md) from the list of sessions. |

### Properties

| Name | Summary |
|---|---|
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
