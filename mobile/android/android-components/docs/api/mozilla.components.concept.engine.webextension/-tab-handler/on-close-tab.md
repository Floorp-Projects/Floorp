[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [TabHandler](index.md) / [onCloseTab](./on-close-tab.md)

# onCloseTab

`open fun onCloseTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L301)

Invoked when a web extension attempts to close a tab via
browser.tabs.remove.

### Parameters

`webExtension` - The [WebExtension](../-web-extension/index.md) that wants to remove the tab.

`engineSession` - then engine session of the tab to be closed.

**Return**
true if the tab was closed, otherwise false.

