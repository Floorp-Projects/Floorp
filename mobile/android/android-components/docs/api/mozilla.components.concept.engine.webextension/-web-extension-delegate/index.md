[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](./index.md)

# WebExtensionDelegate

`interface WebExtensionDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L14)

Notifies applications or other components of engine events related to web
extensions e.g. an extension was installed, or an extension wants to open
a new tab.

### Functions

| Name | Summary |
|---|---|
| [onInstalled](on-installed.md) | `open fun onInstalled(webExtension: `[`WebExtension`](../-web-extension/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web extension was installed successfully. |
| [onNewTab](on-new-tab.md) | `open fun onNewTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web extension attempts to open a new tab via browser.tabs.create. Note that browser.tabs.remove is currently not supported: https://github.com/mozilla-mobile/android-components/issues/4682 |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
