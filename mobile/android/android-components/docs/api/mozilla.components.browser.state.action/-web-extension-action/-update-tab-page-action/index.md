[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UpdateTabPageAction](./index.md)

# UpdateTabPageAction

`data class UpdateTabPageAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L420)

Updates a page action that belongs to the given [sessionId](session-id.md) and [extensionId](extension-id.md) on the
[TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateTabPageAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageAction: `[`WebExtensionPageAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md)`)`<br>Updates a page action that belongs to the given [sessionId](session-id.md) and [extensionId](extension-id.md) on the [TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |

### Properties

| Name | Summary |
|---|---|
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [pageAction](page-action.md) | `val pageAction: `[`WebExtensionPageAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
