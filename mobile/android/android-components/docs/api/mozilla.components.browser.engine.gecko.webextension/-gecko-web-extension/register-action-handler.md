[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](index.md) / [registerActionHandler](./register-action-handler.md)

# registerActionHandler

`fun registerActionHandler(actionHandler: `[`ActionHandler`](../../mozilla.components.concept.engine.webextension/-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L149)

Overrides [WebExtension.registerActionHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-action-handler.md)

Registers an [ActionHandler](../../mozilla.components.concept.engine.webextension/-action-handler/index.md) for this web extension. The handler will
be invoked whenever browser and page action defaults change. To listen
for session-specific overrides see registerActionHandler(
EngineSession, ActionHandler).

`fun registerActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, actionHandler: `[`ActionHandler`](../../mozilla.components.concept.engine.webextension/-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/webextension/GeckoWebExtension.kt#L150)

Overrides [WebExtension.registerActionHandler](../../mozilla.components.concept.engine.webextension/-web-extension/register-action-handler.md)

Registers an [ActionHandler](../../mozilla.components.concept.engine.webextension/-action-handler/index.md) for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). The handler
will be invoked whenever browser and page action overrides are received
for the provided session.

### Parameters

`session` - the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) the handler should be registered for.

`actionHandler` - the [ActionHandler](../../mozilla.components.concept.engine.webextension/-action-handler/index.md) to invoked when a browser or
page action is received.