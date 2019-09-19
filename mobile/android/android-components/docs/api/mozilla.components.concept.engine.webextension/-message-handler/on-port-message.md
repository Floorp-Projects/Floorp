[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [MessageHandler](index.md) / [onPortMessage](./on-port-message.md)

# onPortMessage

`open fun onPortMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, port: `[`Port`](../-port/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L121)

Invoked when a message was received on the provided port.

### Parameters

`message` - the received message, either be a primitive type
or a org.json.JSONObject.

`port` - the port the message was received on.