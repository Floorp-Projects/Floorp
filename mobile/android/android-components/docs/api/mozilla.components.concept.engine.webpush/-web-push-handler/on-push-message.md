[android-components](../../index.md) / [mozilla.components.concept.engine.webpush](../index.md) / [WebPushHandler](index.md) / [onPushMessage](./on-push-message.md)

# onPushMessage

`abstract fun onPushMessage(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, message: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webpush/WebPush.kt#L22)

Invoked when a push message has been delivered.

### Parameters

`scope` - The subscription identifier which usually represents the website's URI.

`message` - A [ByteArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) message.