[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [RemoveTabAction](./index.md)

# RemoveTabAction

`data class RemoveTabAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L37)

Removes the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) from the list of sessions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveTabAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Removes the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) from the list of sessions. |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
