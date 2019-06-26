[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [AddTabAction](./index.md)

# AddTabAction

`data class AddTabAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L27)

Adds a new [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the list.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddTabAction(tab: `[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)`, select: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Adds a new [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the list. |

### Properties

| Name | Summary |
|---|---|
| [select](select.md) | `val select: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [tab](tab.md) | `val tab: `[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) |
