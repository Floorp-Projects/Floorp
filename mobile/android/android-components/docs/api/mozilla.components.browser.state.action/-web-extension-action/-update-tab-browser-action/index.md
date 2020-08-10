[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UpdateTabBrowserAction](./index.md)

# UpdateTabBrowserAction

`data class UpdateTabBrowserAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L410)

Updates a tab-specific browser action that belongs to the given [sessionId](session-id.md) and [extensionId](extension-id.md) on the
[TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateTabBrowserAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, browserAction: `[`WebExtensionBrowserAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)`)`<br>Updates a tab-specific browser action that belongs to the given [sessionId](session-id.md) and [extensionId](extension-id.md) on the [TabSessionState.extensionState](../../../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |

### Properties

| Name | Summary |
|---|---|
| [browserAction](browser-action.md) | `val browserAction: `[`WebExtensionBrowserAction`](../../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md) |
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
