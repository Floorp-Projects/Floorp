[android-components](../../index.md) / [mozilla.components.browser.session.action](../index.md) / [SessionListAction](./index.md)

# SessionListAction

`sealed class SessionListAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/BrowserAction.kt#L20)

[BrowserAction](../-browser-action.md) implementations related to updating the list of [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) inside [BrowserState](../../mozilla.components.browser.session.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [AddSessionAction](-add-session-action/index.md) | `data class AddSessionAction : `[`SessionListAction`](./index.md)<br>Adds a new [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) to the list. |
| [RemoveSessionAction](-remove-session-action/index.md) | `data class RemoveSessionAction : `[`SessionListAction`](./index.md)<br>Removes the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-remove-session-action/session-id.md) from the list of sessions. |
| [SelectSessionAction](-select-session-action/index.md) | `data class SelectSessionAction : `[`SessionListAction`](./index.md)<br>Marks the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-select-session-action/session-id.md) as selected session. |

### Inheritors

| Name | Summary |
|---|---|
| [AddSessionAction](-add-session-action/index.md) | `data class AddSessionAction : `[`SessionListAction`](./index.md)<br>Adds a new [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) to the list. |
| [RemoveSessionAction](-remove-session-action/index.md) | `data class RemoveSessionAction : `[`SessionListAction`](./index.md)<br>Removes the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-remove-session-action/session-id.md) from the list of sessions. |
| [SelectSessionAction](-select-session-action/index.md) | `data class SelectSessionAction : `[`SessionListAction`](./index.md)<br>Marks the [SessionState](../../mozilla.components.browser.session.state/-session-state/index.md) with the given [sessionId](-select-session-action/session-id.md) as selected session. |
