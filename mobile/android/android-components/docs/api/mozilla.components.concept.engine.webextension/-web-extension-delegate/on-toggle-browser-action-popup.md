[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onToggleBrowserActionPopup](./on-toggle-browser-action-popup.md)

# onToggleBrowserActionPopup

`open fun onToggleBrowserActionPopup(webExtension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, action: `[`BrowserAction`](../-browser-action/index.md)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L70)

Invoked when a browser action wants to toggle a popup view.

### Parameters

`webExtension` - The [WebExtension](../-web-extension/index.md) that wants to display the popup.

`engineSession` - The [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to use for displaying the popup.

`action` - the [BrowserAction](../-browser-action/index.md) that defines the popup.

**Return**
the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) used to display the popup, or null if no popup
was displayed.

