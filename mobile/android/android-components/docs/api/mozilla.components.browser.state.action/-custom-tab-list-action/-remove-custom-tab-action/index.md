[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [CustomTabListAction](../index.md) / [RemoveCustomTabAction](./index.md)

# RemoveCustomTabAction

`data class RemoveCustomTabAction : `[`CustomTabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L114)

Removes an existing [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to [BrowserState.customTabs](../../../mozilla.components.browser.state.state/-browser-state/custom-tabs.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoveCustomTabAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Removes an existing [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to [BrowserState.customTabs](../../../mozilla.components.browser.state.state/-browser-state/custom-tabs.md). |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of the custom tab to remove. |
