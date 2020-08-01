[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionDelegate](index.md) / [onPageActionDefined](./on-page-action-defined.md)

# onPageActionDefined

`open fun onPageActionDefined(extension: `[`WebExtension`](../-web-extension/index.md)`, action: `[`Action`](../-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionDelegate.kt#L83)

Invoked when a web extension defines a page action. To listen for session-specific
overrides of [Action](../-action/index.md)s and other action-specific events (e.g. opening a popup)
see [WebExtension.registerActionHandler](../-web-extension/register-action-handler.md).

### Parameters

`extension` - The [WebExtension](../-web-extension/index.md) defining the browser action.

`action` - the defined page [Action](../-action/index.md).