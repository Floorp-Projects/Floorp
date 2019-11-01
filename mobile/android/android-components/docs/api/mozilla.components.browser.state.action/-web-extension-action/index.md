[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [WebExtensionAction](./index.md)

# WebExtensionAction

`sealed class WebExtensionAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L266)

[BrowserAction](../-browser-action.md) implementations related to updating [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md) and
[TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Types

| Name | Summary |
|---|---|
| [InstallWebExtension](-install-web-extension/index.md) | `data class InstallWebExtension : `[`WebExtensionAction`](./index.md)<br>Installs the given [extension](-install-web-extension/extension.md) and adds it to the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [InstallWebExtension](-install-web-extension/index.md) | `data class InstallWebExtension : `[`WebExtensionAction`](./index.md)<br>Installs the given [extension](-install-web-extension/extension.md) and adds it to the [BrowserState.extensions](../../mozilla.components.browser.state.state/-browser-state/extensions.md). |
| [UpdateBrowserAction](-update-browser-action/index.md) | `data class UpdateBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action of a given [extensionId](-update-browser-action/extension-id.md). |
| [UpdateTabBrowserAction](-update-tab-browser-action/index.md) | `data class UpdateTabBrowserAction : `[`WebExtensionAction`](./index.md)<br>Updates a browser action that belongs to the given [sessionId](-update-tab-browser-action/session-id.md) and [extensionId](-update-tab-browser-action/extension-id.md) on the [TabSessionState.extensionState](../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
