[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UninstallWebExtensionAction](./index.md)

# UninstallWebExtensionAction

`data class UninstallWebExtensionAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L356)

Removes all state of the uninstalled extension from [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md)
and [TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UninstallWebExtensionAction(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Removes all state of the uninstalled extension from [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |

### Properties

| Name | Summary |
|---|---|
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
