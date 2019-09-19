[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [sendContentMessage](./send-content-message.md)

# sendContentMessage

`fun sendContentMessage(msg: <ERROR CLASS>, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = extensionId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L85)

Sends a content message to the provided session.

### Parameters

`msg` - the message to send

`engineSession` - the session to send the content message to.

`name` - (optional) name of the port, defaults to the provided extensionId.