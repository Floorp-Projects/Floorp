[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [TabHandler](index.md) / [onNewTab](./on-new-tab.md)

# onNewTab

`open fun onNewTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L279)

Invoked when a web extension attempts to open a new tab via
browser.tabs.create.

### Parameters

`webExtension` - The [WebExtension](../-web-extension/index.md) that wants to open the tab.

`engineSession` - an instance of engine session to open a new tab with.

`active` - whether or not the new tab should be active/selected.

`url` - the target url to be loaded in a new tab.