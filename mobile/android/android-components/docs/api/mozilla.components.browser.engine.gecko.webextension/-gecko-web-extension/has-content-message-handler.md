[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](index.md) / [hasContentMessageHandler](./has-content-message-handler.md)

# hasContentMessageHandler

`fun hasContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L29)

Overrides [WebExtension.hasContentMessageHandler](../../mozilla.components.concept.engine.webextension/-web-extension/has-content-message-handler.md)

Checks whether there is an existing content message handler for the provided
session and "application" name.

### Parameters

`session` - the session the message handler was registered for.

`name` - the "application" name the message handler was registered for.

**Return**
true if a content message handler is active, otherwise false.

