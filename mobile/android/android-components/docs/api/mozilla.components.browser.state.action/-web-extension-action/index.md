[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [WebExtensionAction](./index.md)

# WebExtensionAction

`sealed class WebExtensionAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L267)

[BrowserAction](../-browser-action.md) implementations related to updating [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and
[TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Types

| Name | Summary |
|---|---|
| [InstallWebExtensionAction](-install-web-extension-action/index.md) | `data class InstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](-install-web-extension-action/extension.md) as installed. |
| [UninstallWebExtensionAction](-uninstall-web-extension-action/index.md) | `data class UninstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Removes all state of the uninstalled extension from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdateBrowserActionPopupSession](-update-browser-action-popup-session/index.md) | `data class UpdateBrowserActionPopupSession : `[`WebExtensionAction`](./index.md)<br>Keeps track of the last session used to display the [BrowserAction](../-browser-action.md) popup. |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateWebExtension](-update-web-extension/index.md) | `data class UpdateWebExtension : `[`WebExtensionAction`](./index.md)<br>Update the given [updatedExtension](-update-web-extension/updated-extension.md) in the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateWebExtensionEnabledAction](-update-web-extension-enabled-action/index.md) | `data class UpdateWebExtensionEnabledAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [InstallWebExtensionAction](-install-web-extension-action/index.md) | `data class InstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](-install-web-extension-action/extension.md) as installed. |
| [UninstallWebExtensionAction](-uninstall-web-extension-action/index.md) | `data class UninstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Removes all state of the uninstalled extension from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdateBrowserActionPopupSession](-update-browser-action-popup-session/index.md) | `data class UpdateBrowserActionPopupSession : `[`WebExtensionAction`](./index.md)<br>Keeps track of the last session used to display the [BrowserAction](../-browser-action.md) popup. |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateWebExtension](-update-web-extension/index.md) | `data class UpdateWebExtension : `[`WebExtensionAction`](./index.md)<br>Update the given [updatedExtension](-update-web-extension/updated-extension.md) in the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateWebExtensionEnabledAction](-update-web-extension-enabled-action/index.md) | `data class UpdateWebExtensionEnabledAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |
