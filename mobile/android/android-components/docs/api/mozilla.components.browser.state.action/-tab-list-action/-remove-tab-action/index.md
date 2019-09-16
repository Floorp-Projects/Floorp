[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [RemoveTabAction](./index.md)

# RemoveTabAction

`data class RemoveTabAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L72)

Removes the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) from the list of sessions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveTabAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectParentIfExists: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Removes the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) from the list of sessions. |

### Properties

| Name | Summary |
|---|---|
| [selectParentIfExists](select-parent-if-exists.md) | `val selectParentIfExists: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not a parent tab should be selected if one exists, defaults to true. |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of the tab to remove. |
