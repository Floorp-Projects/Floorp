[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [registerBackgroundMessageHandler](./register-background-message-handler.md)

# registerBackgroundMessageHandler

`fun registerBackgroundMessageHandler(messageHandler: `[`MessageHandler`](../../mozilla.components.concept.engine.webextension/-message-handler/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L99)

Registers a background message handler for this extension. An existing handler
will be replaced and there is no need to unregister.

### Parameters

`messageHandler` - the message handler to register.

`name` - (optional) name of the port, defaults to the provided messagingId.