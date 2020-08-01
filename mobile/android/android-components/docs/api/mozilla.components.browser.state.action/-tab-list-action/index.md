[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [TabListAction](./index.md)

# TabListAction

`sealed class TabListAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L62)

[BrowserAction](../-browser-action.md) implementations related to updating the list of [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) inside [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [AddMultipleTabsAction](-add-multiple-tabs-action/index.md) | `data class AddMultipleTabsAction : `[`TabListAction`](./index.md)<br>Adds multiple [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) objects to the [BrowserState.tabs](../../mozilla.components.browser.state.state/-browser-state/tabs.md) list. |
| [AddTabAction](-add-tab-action/index.md) | `data class AddTabAction : `[`TabListAction`](./index.md)<br>Adds a new [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the [BrowserState.tabs](../../mozilla.components.browser.state.state/-browser-state/tabs.md) list. |
| [RemoveAllNormalTabsAction](-remove-all-normal-tabs-action.md) | `object RemoveAllNormalTabsAction : `[`TabListAction`](./index.md)<br>Removes all non-private [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveAllPrivateTabsAction](-remove-all-private-tabs-action.md) | `object RemoveAllPrivateTabsAction : `[`TabListAction`](./index.md)<br>Removes all private [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveAllTabsAction](-remove-all-tabs-action.md) | `object RemoveAllTabsAction : `[`TabListAction`](./index.md)<br>Removes both private and normal [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveTabAction](-remove-tab-action/index.md) | `data class RemoveTabAction : `[`TabListAction`](./index.md)<br>Removes the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-remove-tab-action/tab-id.md) from the list of sessions. |
| [RestoreAction](-restore-action/index.md) | `data class RestoreAction : `[`TabListAction`](./index.md)<br>Restores state from a (partial) previous state. |
| [SelectTabAction](-select-tab-action/index.md) | `data class SelectTabAction : `[`TabListAction`](./index.md)<br>Marks the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-select-tab-action/tab-id.md) as selected tab. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddMultipleTabsAction](-add-multiple-tabs-action/index.md) | `data class AddMultipleTabsAction : `[`TabListAction`](./index.md)<br>Adds multiple [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) objects to the [BrowserState.tabs](../../mozilla.components.browser.state.state/-browser-state/tabs.md) list. |
| [AddTabAction](-add-tab-action/index.md) | `data class AddTabAction : `[`TabListAction`](./index.md)<br>Adds a new [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) to the [BrowserState.tabs](../../mozilla.components.browser.state.state/-browser-state/tabs.md) list. |
| [RemoveAllNormalTabsAction](-remove-all-normal-tabs-action.md) | `object RemoveAllNormalTabsAction : `[`TabListAction`](./index.md)<br>Removes all non-private [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveAllPrivateTabsAction](-remove-all-private-tabs-action.md) | `object RemoveAllPrivateTabsAction : `[`TabListAction`](./index.md)<br>Removes all private [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveAllTabsAction](-remove-all-tabs-action.md) | `object RemoveAllTabsAction : `[`TabListAction`](./index.md)<br>Removes both private and normal [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md)s. |
| [RemoveTabAction](-remove-tab-action/index.md) | `data class RemoveTabAction : `[`TabListAction`](./index.md)<br>Removes the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-remove-tab-action/tab-id.md) from the list of sessions. |
| [RestoreAction](-restore-action/index.md) | `data class RestoreAction : `[`TabListAction`](./index.md)<br>Restores state from a (partial) previous state. |
| [SelectTabAction](-select-tab-action/index.md) | `data class SelectTabAction : `[`TabListAction`](./index.md)<br>Marks the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-select-tab-action/tab-id.md) as selected tab. |
