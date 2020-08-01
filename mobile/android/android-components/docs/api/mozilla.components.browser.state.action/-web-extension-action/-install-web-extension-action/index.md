[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [InstallWebExtensionAction](./index.md)

# InstallWebExtensionAction

`data class InstallWebExtensionAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L350)

Updates [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](extension.md) as installed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InstallWebExtensionAction(extension: `[`WebExtensionState`](../../../mozilla.components.browser.state.state/-web-extension-state/index.md)`)`<br>Updates [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md) to register the given [extension](extension.md) as installed. |

### Properties

| Name | Summary |
|---|---|
| [extension](extension.md) | `val extension: `[`WebExtensionState`](../../../mozilla.components.browser.state.state/-web-extension-state/index.md) |
