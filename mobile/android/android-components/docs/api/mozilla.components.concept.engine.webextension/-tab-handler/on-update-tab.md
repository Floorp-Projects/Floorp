[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [TabHandler](index.md) / [onUpdateTab](./on-update-tab.md)

# onUpdateTab

`open fun onUpdateTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L291)

Invoked when a web extension attempts to update a tab via
browser.tabs.update.

### Parameters

`webExtension` - The [WebExtension](../-web-extension/index.md) that wants to update the tab.

`engineSession` - an instance of engine session to open a new tab with.

`active` - whether or not the new tab should be active/selected.

`url` - the (optional) target url to be loaded in a new tab if it has changed.

**Return**
true if the tab was updated, otherwise false.

