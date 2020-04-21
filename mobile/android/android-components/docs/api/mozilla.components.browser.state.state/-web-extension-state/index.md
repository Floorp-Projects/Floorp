[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [WebExtensionState](./index.md)

# WebExtensionState

`data class WebExtensionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/WebExtensionState.kt#L27)

Value type that represents the state of a web extension.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, allowedInPrivateBrowsing: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, browserAction: `[`WebExtensionBrowserAction`](../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)`? = null, pageAction: `[`WebExtensionPageAction`](../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md)`? = null, popupSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, popupSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null)`<br>Value type that represents the state of a web extension. |

### Properties

| Name | Summary |
|---|---|
| [allowedInPrivateBrowsing](allowed-in-private-browsing.md) | `val allowedInPrivateBrowsing: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not this web extension is allowed in private browsing mode. Defaults to false. |
| [browserAction](browser-action.md) | `val browserAction: `[`WebExtensionBrowserAction`](../../mozilla.components.concept.engine.webextension/-web-extension-browser-action.md)`?`<br>The browser action state of this extension. |
| [enabled](enabled.md) | `val enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not this web extension is enabled, defaults to true. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The unique identifier for this web extension. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The name of this web extension. |
| [pageAction](page-action.md) | `val pageAction: `[`WebExtensionPageAction`](../../mozilla.components.concept.engine.webextension/-web-extension-page-action.md)`?`<br>The page action state of this extension. |
| [popupSession](popup-session.md) | `val popupSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>The [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) displaying the browser or page action popup. |
| [popupSessionId](popup-session-id.md) | `val popupSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The ID of the session displaying the browser action popup. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
