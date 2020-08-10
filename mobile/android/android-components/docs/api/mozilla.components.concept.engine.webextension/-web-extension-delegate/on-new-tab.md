[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onNewTab](./on-new-tab.md)

# onNewTab

`open fun onNewTab(extension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L63)

Invoked when a web extension attempts to open a new tab via
browser.tabs.create. Note that browser.tabs.update and browser.tabs.remove
can only be observed using session-specific handlers,
see [WebExtension.registerTabHandler](../-web-extension/register-tab-handler.md).

### Parameters

`extension` - The [WebExtension](../-web-extension/index.md) that wants to open a new tab.

`engineSession` - an instance of engine session to open a new tab with.

`active` - whether or not the new tab should be active/selected.

`url` - the target url to be loaded in a new tab.