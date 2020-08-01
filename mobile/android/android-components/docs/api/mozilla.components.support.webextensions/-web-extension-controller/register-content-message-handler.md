[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [registerContentMessageHandler](./register-content-message-handler.md)

# registerContentMessageHandler

`fun registerContentMessageHandler(engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L78)

Registers a content message handler for the provided session. Currently only one
handler can be registered per session. An existing handler will be replaced and
there is no need to unregister.

### Parameters

`engineSession` - the session the content message handler should be registered with.

`messageHandler` - the message handler to register.

`name` - (optional) name of the port, defaults to the provided messagingId.