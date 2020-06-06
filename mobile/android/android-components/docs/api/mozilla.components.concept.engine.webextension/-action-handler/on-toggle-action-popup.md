[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [ActionHandler](index.md) / [onToggleActionPopup](./on-toggle-action-popup.md)

# onToggleActionPopup

`open fun onToggleActionPopup(extension: `[`WebExtension`](../-web-extension/index.md)`, action: `[`Action`](../-action/index.md)`): `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L211)

Invoked when a browser or page action wants to toggle a popup view.

### Parameters

`extension` - the extension that defined the browser or page action.

`action` - the action as [Action](../-action/index.md).

**Return**
the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) that was used for displaying the popup,
or null if the popup was closed.

