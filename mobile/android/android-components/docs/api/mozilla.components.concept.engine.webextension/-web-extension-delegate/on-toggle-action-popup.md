[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onToggleActionPopup](./on-toggle-action-popup.md)

# onToggleActionPopup

`open fun onToggleActionPopup(extension: `[`WebExtension`](../-web-extension/index.md)`, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, action: `[`Action`](../-action/index.md)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L94)

Invoked when a browser or page action wants to toggle a popup view.

### Parameters

`extension` - The [WebExtension](../-web-extension/index.md) that wants to display the popup.

`engineSession` - The [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to use for displaying the popup.

`action` - the [Action](../-action/index.md) that defines the popup.

**Return**
the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) used to display the popup, or null if no popup
was displayed.

