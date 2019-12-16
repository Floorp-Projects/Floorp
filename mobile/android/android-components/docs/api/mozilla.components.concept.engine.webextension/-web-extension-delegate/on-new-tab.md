[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onNewTab](./on-new-tab.md)

# onNewTab

`open fun onNewTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L53)

Invoked when a web extension attempts to open a new tab via
browser.tabs.create.

### Parameters

`webExtension` - An instance of [WebExtension](../-web-extension/index.md) or null if extension
was not registered with the engine.

`url` - the target url to be loaded in a new tab.

`engineSession` - an instance of engine session to open a new tab with.