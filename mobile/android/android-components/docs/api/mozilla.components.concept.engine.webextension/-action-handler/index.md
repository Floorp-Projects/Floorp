[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [ActionHandler](./index.md)

# ActionHandler

`interface ActionHandler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L138)

A handler for web extension (browser and page) actions.

Page action support will be addressed in:
https://github.com/mozilla-mobile/android-components/issues/4470

### Functions

| Name | Summary |
|---|---|
| [onBrowserAction](on-browser-action.md) | `open fun onBrowserAction(extension: `[`WebExtension`](../-web-extension/index.md)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, action: `[`BrowserAction`](../-browser-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a browser action is defined or updated. |
| [onToggleBrowserActionPopup](on-toggle-browser-action-popup.md) | `open fun onToggleBrowserActionPopup(extension: `[`WebExtension`](../-web-extension/index.md)`, action: `[`BrowserAction`](../-browser-action/index.md)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?`<br>Invoked when a browser action wants to toggle a popup view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
