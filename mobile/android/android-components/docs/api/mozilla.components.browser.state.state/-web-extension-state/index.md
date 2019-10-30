[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [WebExtensionState](./index.md)

# WebExtensionState

`data class WebExtensionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/WebExtensionState.kt#L17)

Value type that represents the state of a web extension.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtensionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, browserAction: `[`WebExtensionBrowserAction`](../-web-extension-browser-action.md)`? = null)`<br>Value type that represents the state of a web extension. |

### Properties

| Name | Summary |
|---|---|
| [browserAction](browser-action.md) | `val browserAction: `[`WebExtensionBrowserAction`](../-web-extension-browser-action.md)`?`<br>A list browser action that this web extension has. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The unique identifier for this web extension. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
