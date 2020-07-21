[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [EncryptedPushMessage](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`EncryptedPushMessage`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushProcessor.kt#L85)

The [salt](invoke.md#mozilla.components.concept.push.EncryptedPushMessage.Companion$invoke(kotlin.String, kotlin.String, kotlin.String, kotlin.String, kotlin.String)/salt) and [cryptoKey](invoke.md#mozilla.components.concept.push.EncryptedPushMessage.Companion$invoke(kotlin.String, kotlin.String, kotlin.String, kotlin.String, kotlin.String)/cryptoKey) are optional as part of the standard for WebPush, so we should default
to empty strings.

Note: We have use the invoke operator since secondary constructors don't work with nullable primitive types.

