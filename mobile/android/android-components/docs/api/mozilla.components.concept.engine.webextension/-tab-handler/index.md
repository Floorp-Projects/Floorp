[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [TabHandler](./index.md)

# TabHandler

`interface TabHandler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L268)

A handler for all tab related events (triggered by browser.tabs.* methods).

### Functions

| Name | Summary |
|---|---|
| [onCloseTab](on-close-tab.md) | `open fun onCloseTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invoked when a web extension attempts to close a tab via browser.tabs.remove. |
| [onNewTab](on-new-tab.md) | `open fun onNewTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web extension attempts to open a new tab via browser.tabs.create. |
| [onUpdateTab](on-update-tab.md) | `open fun onUpdateTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Invoked when a web extension attempts to update a tab via browser.tabs.update. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
