[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [MessageHandler](index.md) / [onMessage](./on-message.md)

# onMessage

`open fun onMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, source: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`?): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L262)

Invoked when a message was received as a result of a
`browser.runtime.sendNativeMessage` call in JavaScript.

### Parameters

`message` - the received message, either be a primitive type
or a org.json.JSONObject.

`source` - the session this message originated from if from a content
script, otherwise null.

**Return**
the response to be sent for this message, either a primitive
type or a org.json.JSONObject, null if no response should be sent.

