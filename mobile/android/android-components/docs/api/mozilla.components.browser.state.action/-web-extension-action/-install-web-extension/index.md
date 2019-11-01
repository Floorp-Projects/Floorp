[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [InstallWebExtension](./index.md)

# InstallWebExtension

`data class InstallWebExtension : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L270)

Installs the given [extension](extension.md) and adds it to the [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InstallWebExtension(extension: `[`WebExtensionState`](../../../mozilla.components.browser.state.state/-web-extension-state/index.md)`)`<br>Installs the given [extension](extension.md) and adds it to the [BrowserState.extensions](../../../mozilla.components.browser.state.state/-browser-state/extensions.md). |

### Properties

| Name | Summary |
|---|---|
| [extension](extension.md) | `val extension: `[`WebExtensionState`](../../../mozilla.components.browser.state.state/-web-extension-state/index.md) |
