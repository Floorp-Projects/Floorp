[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [hasActionHandler](./has-action-handler.md)

# hasActionHandler

`abstract fun hasActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L119)

Checks whether there is an existing action handler for the provided
session.

### Parameters

`session` - the session the action handler was registered for.

**Return**
true if an action handler is registered, otherwise false.

