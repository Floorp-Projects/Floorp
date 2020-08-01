[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UpdatePageAction](./index.md)

# UpdatePageAction

`data class UpdatePageAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L392)

Updates a page action of a given [extensionId](extension-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdatePageAction(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageAction: `[`WebExtensionPageAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md)`)`<br>Updates a page action of a given [extensionId](extension-id.md). |

### Properties

| Name | Summary |
|---|---|
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [pageAction](page-action.md) | `val pageAction: `[`WebExtensionPageAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md) |
