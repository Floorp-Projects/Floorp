[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [Port](index.md) / [postMessage](./post-message.md)

# postMessage

`abstract fun postMessage(message: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L126)

Sends a message to this port.

### Parameters

`message` - the message to send, either a primitive type
or a org.json.JSONObject.