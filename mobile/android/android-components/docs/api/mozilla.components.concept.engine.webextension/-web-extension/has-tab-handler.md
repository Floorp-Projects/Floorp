[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [hasTabHandler](./has-tab-handler.md)

# hasTabHandler

`abstract fun hasTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L148)

Checks whether there is an existing tab handler for the provided
session.

### Parameters

`session` - the session the tab handler was registered for.

**Return**
true if an tab handler is registered, otherwise false.

