[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [CustomTabListAction](../index.md) / [TurnCustomTabIntoNormalTabAction](./index.md)

# TurnCustomTabIntoNormalTabAction

`data class TurnCustomTabIntoNormalTabAction : `[`CustomTabListAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L139)

Converts an existing [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to a regular/normal [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TurnCustomTabIntoNormalTabAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Converts an existing [CustomTabSessionState](../../../mozilla.components.browser.state.state/-custom-tab-session-state/index.md) to a regular/normal [TabSessionState](../../../mozilla.components.browser.state.state/-tab-session-state/index.md). |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
