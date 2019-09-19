[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [AddTabAction](./index.md)

# AddTabAction

`data class AddTabAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L51)

Adds a new [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the [BrowserState.tabs](../../../mozilla.components.browser.state.state/-browser-state/tabs.md) list.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddTabAction(tab: `[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)`, select: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Adds a new [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the [BrowserState.tabs](../../../mozilla.components.browser.state.state/-browser-state/tabs.md) list. |

### Properties

| Name | Summary |
|---|---|
| [select](select.md) | `val select: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>whether or not to the tab should be selected. |
| [tab](tab.md) | `val tab: `[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)<br>the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) to add |
