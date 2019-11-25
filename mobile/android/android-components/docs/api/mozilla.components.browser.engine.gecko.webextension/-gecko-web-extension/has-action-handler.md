[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](index.md) / [hasActionHandler](./has-action-handler.md)

# hasActionHandler

`fun hasActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L151)

Overrides [WebExtension.hasActionHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-action-handler.md)

Checks whether there is an existing action handler for the provided
session.

### Parameters

`session` - the session the action handler was registered for.

**Return**
true if an action handler is registered, otherwise false.

