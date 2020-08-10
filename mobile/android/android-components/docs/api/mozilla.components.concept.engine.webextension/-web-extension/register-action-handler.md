[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [registerActionHandler](./register-action-handler.md)

# registerActionHandler

`abstract fun registerActionHandler(actionHandler: `[`ActionHandler`](../-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L99)

Registers an [ActionHandler](../-action-handler/index.md) for this web extension. The handler will
be invoked whenever browser and page action defaults change. To listen
for session-specific overrides see registerActionHandler(
EngineSession, ActionHandler).

### Parameters

`actionHandler` - the [ActionHandler](../-action-handler/index.md) to be invoked when a browser or
page action is received.`abstract fun registerActionHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, actionHandler: `[`ActionHandler`](../-action-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L110)

Registers an [ActionHandler](../-action-handler/index.md) for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). The handler
will be invoked whenever browser and page action overrides are received
for the provided session.

### Parameters

`session` - the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) the handler should be registered for.

`actionHandler` - the [ActionHandler](../-action-handler/index.md) to be invoked when a
session-specific browser or page action is received.