[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [ActionHandler](index.md) / [onPageAction](./on-page-action.md)

# onPageAction

`open fun onPageAction(extension: `[`WebExtension`](../-web-extension/index.md)`, session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, action: `[`Action`](../-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L201)

Invoked when a page action is defined or updated.

### Parameters

`extension` - the extension that defined the browser action.

`session` - the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) if this action is to be updated for a
specific session, or null if this is to set a new default value.

`action` - the [Action](../-action/index.md)