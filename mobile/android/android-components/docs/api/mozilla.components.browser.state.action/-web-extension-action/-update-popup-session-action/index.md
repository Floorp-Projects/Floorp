[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [WebExtensionAction](../index.md) / [UpdatePopupSessionAction](./index.md)

# UpdatePopupSessionAction

`data class UpdatePopupSessionAction : `[`WebExtensionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L400)

Keeps track of the last session used to display an extension action popup.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdatePopupSessionAction(extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, popupSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, popupSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Keeps track of the last session used to display an extension action popup. |

### Properties

| Name | Summary |
|---|---|
| [extensionId](extension-id.md) | `val extensionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [popupSession](popup-session.md) | `val popupSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`?` |
| [popupSessionId](popup-session-id.md) | `val popupSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
