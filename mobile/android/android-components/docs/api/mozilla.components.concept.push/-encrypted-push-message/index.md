[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [EncryptedPushMessage](./index.md)

# EncryptedPushMessage

`data class EncryptedPushMessage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushProcessor.kt#L71)

A push message holds the information needed to pass the message on to the appropriate receiver.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EncryptedPushMessage(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "")`<br>A push message holds the information needed to pass the message on to the appropriate receiver. |

### Properties

| Name | Summary |
|---|---|
| [body](body.md) | `val body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [channelId](channel-id.md) | `val channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [cryptoKey](crypto-key.md) | `val cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [encoding](encoding.md) | `val encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [salt](salt.md) | `val salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`EncryptedPushMessage`](./index.md)<br>The [salt](invoke.md#mozilla.components.concept.push.EncryptedPushMessage.Companion$invoke(kotlin.String, kotlin.String, kotlin.String, kotlin.String, kotlin.String)/salt) and [cryptoKey](invoke.md#mozilla.components.concept.push.EncryptedPushMessage.Companion$invoke(kotlin.String, kotlin.String, kotlin.String, kotlin.String, kotlin.String)/cryptoKey) are optional as part of the standard for WebPush, so we should default to empty strings. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
