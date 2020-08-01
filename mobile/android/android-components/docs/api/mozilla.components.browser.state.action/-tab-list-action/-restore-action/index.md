[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [RestoreAction](./index.md)

# RestoreAction

`data class RestoreAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L98)

Restores state from a (partial) previous state.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RestoreAction(tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)`>, selectedTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Restores state from a (partial) previous state. |

### Properties

| Name | Summary |
|---|---|
| [selectedTabId](selected-tab-id.md) | `val selectedTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the ID of the tab to select. |
| [tabs](tabs.md) | `val tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabSessionState`](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)`>`<br>the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md)s to restore. |
