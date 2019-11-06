[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onCloseTab](./on-close-tab.md)

# onCloseTab

`open fun onCloseTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`?, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L42)

Invoked when a web extension attempts to close a tab via browser.tabs.remove.

### Parameters

`webExtension` - An instance of [WebExtension](../-web-extension/index.md) or null if extension
was not registered with the engine.

`engineSession` - then engine session fo the tab to be closed.

**Return**
true if the tab was closed, otherwise false.

