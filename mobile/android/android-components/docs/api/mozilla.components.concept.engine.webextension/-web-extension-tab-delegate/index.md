[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionTabDelegate](./index.md)

# WebExtensionTabDelegate

`interface WebExtensionTabDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionTabDelegate.kt#L14)

Notifies applications / other components that a web extension wants to open a new tab via
browser.tabs.create. Note that browser.tabs.remove is currently not supported:
https://github.com/mozilla-mobile/android-components/issues/4682

### Functions

| Name | Summary |
|---|---|
| [onNewTab](on-new-tab.md) | `open fun onNewTab(webExtension: `[`WebExtension`](../-web-extension/index.md)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web extension opens a new tab. |
