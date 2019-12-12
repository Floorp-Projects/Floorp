[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [ActionHandler](index.md) / [onToggleBrowserActionPopup](./on-toggle-browser-action-popup.md)

# onToggleBrowserActionPopup

`open fun onToggleBrowserActionPopup(extension: `[`WebExtension`](../-web-extension/index.md)`, action: `[`BrowserAction`](../-browser-action/index.md)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L158)

Invoked when a browser action wants to toggle a popup view.

### Parameters

`extension` - the extension that defined the browser action.

`action` - the [BrowserAction](../-browser-action/index.md)

**Return**
the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) that was used for displaying the popup,
or null if the popup was closed.

