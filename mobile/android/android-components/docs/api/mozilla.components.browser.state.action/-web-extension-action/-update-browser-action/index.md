[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UpdateBrowserAction](./index.md)

# UpdateBrowserAction

`data class UpdateBrowserAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L384)

Updates a browser action of a given [extensionId](extension-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateBrowserAction(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, browserAction: `[`WebExtensionBrowserAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)`)`<br>Updates a browser action of a given [extensionId](extension-id.md). |

### Properties

| Name | Summary |
|---|---|
| [browserAction](browser-action.md) | `val browserAction: `[`WebExtensionBrowserAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md) |
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
