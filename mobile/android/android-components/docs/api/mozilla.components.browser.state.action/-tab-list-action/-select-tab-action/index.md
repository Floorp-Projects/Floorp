[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TabListAction](../index.md) / [SelectTabAction](./index.md)

# SelectTabAction

`data class SelectTabAction : `[`TabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L63)

Marks the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) as selected tab.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SelectTabAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Marks the [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](tab-id.md) as selected tab. |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of the tab to select. |
