[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [CustomTabListAction](../index.md) / [AddCustomTabAction](./index.md)

# AddCustomTabAction

`data class AddCustomTabAction : `[`CustomTabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L125)

Adds a new [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to [BrowserState.customTabs](../../../mozilla.components.browser.state.state/-browser-state/custom-tabs.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddCustomTabAction(tab: `[`CustomTabSessionState`](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md)`)`<br>Adds a new [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to [BrowserState.customTabs](../../../mozilla.components.browser.state.state/-browser-state/custom-tabs.md). |

### Properties

| Name | Summary |
|---|---|
| [tab](tab.md) | `val tab: `[`CustomTabSessionState`](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md)<br>the [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to add. |
