[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [WebExtensionAction](./index.md)

# WebExtensionAction

`sealed class WebExtensionAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L346)

[BrowserAction](../-browser-action.md) implementations related to updating [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and
[TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Types

| Name | Summary |
|---|---|
| [InstallWebExtensionAction](-install-web-extension-action/index.md) | `data class InstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](-install-web-extension-action/extension.md) as installed. |
| [UninstallAllWebExtensionsAction](-uninstall-all-web-extensions-action.md) | `object UninstallAllWebExtensionsAction : `[`WebExtensionAction`](./index.md)<br>Removes state of all extensions from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UninstallWebExtensionAction](-uninstall-web-extension-action/index.md) | `data class UninstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Removes all state of the uninstalled extension from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdatePageAction](-update-page-action/index.md) | `data class UpdatePageAction : `[`WebExtensionAction`](./index.md)<br>Updates a page action of a given [extensionId](-update-page-action/extension-id.md). |
| [UpdatePopupSessionAction](-update-popup-session-action/index.md) | `data class UpdatePopupSessionAction : `[`WebExtensionAction`](./index.md)<br>Keeps track of the last session used to display an extension action popup. |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a tab-specific browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateTabPageAction](-update-tab-page-action/index.md) | `data class UpdateTabPageAction : `[`WebExtensionAction`](./index.md)<br>Updates a page action that belongs to the given [sessionId](-update-tab-page-action/session-id.md) and [extensionId](-update-tab-page-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateWebExtensionAction](-update-web-extension-action/index.md) | `data class UpdateWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates the given [updatedExtension](-update-web-extension-action/updated-extension.md) in the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateWebExtensionAllowedInPrivateBrowsingAction](-update-web-extension-allowed-in-private-browsing-action/index.md) | `data class UpdateWebExtensionAllowedInPrivateBrowsingAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |
| [UpdateWebExtensionEnabledAction](-update-web-extension-enabled-action/index.md) | `data class UpdateWebExtensionEnabledAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [InstallWebExtensionAction](-install-web-extension-action/index.md) | `data class InstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](-install-web-extension-action/extension.md) as installed. |
| [UninstallAllWebExtensionsAction](-uninstall-all-web-extensions-action.md) | `object UninstallAllWebExtensionsAction : `[`WebExtensionAction`](./index.md)<br>Removes state of all extensions from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UninstallWebExtensionAction](-uninstall-web-extension-action/index.md) | `data class UninstallWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Removes all state of the uninstalled extension from [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdatePageAction](-update-page-action/index.md) | `data class UpdatePageAction : `[`WebExtensionAction`](./index.md)<br>Updates a page action of a given [extensionId](-update-page-action/extension-id.md). |
| [UpdatePopupSessionAction](-update-popup-session-action/index.md) | `data class UpdatePopupSessionAction : `[`WebExtensionAction`](./index.md)<br>Keeps track of the last session used to display an extension action popup. |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a tab-specific browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateTabPageAction](-update-tab-page-action/index.md) | `data class UpdateTabPageAction : `[`WebExtensionAction`](./index.md)<br>Updates a page action that belongs to the given [sessionId](-update-tab-page-action/session-id.md) and [extensionId](-update-tab-page-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
| [UpdateWebExtensionAction](-update-web-extension-action/index.md) | `data class UpdateWebExtensionAction : `[`WebExtensionAction`](./index.md)<br>Updates the given [updatedExtension](-update-web-extension-action/updated-extension.md) in the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateWebExtensionAllowedInPrivateBrowsingAction](-update-web-extension-allowed-in-private-browsing-action/index.md) | `data class UpdateWebExtensionAllowedInPrivateBrowsingAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |
| [UpdateWebExtensionEnabledAction](-update-web-extension-enabled-action/index.md) | `data class UpdateWebExtensionEnabledAction : `[`WebExtensionAction`](./index.md)<br>Updates the [WebExtensionState.enabled](../../mozilla.components.browser.state.state/-web-extension-state/enabled.md) flag. |
