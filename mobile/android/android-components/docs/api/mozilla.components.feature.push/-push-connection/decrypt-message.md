[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConnection](index.md) / [decryptMessage](./decrypt-message.md)

# decryptMessage

`abstract suspend fun decryptMessage(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`DecryptedMessage`](../-decrypted-message/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/Connection.kt#L78)

Decrypts a received message.

**Return**
a pair of the push scope and the decrypted message, respectively, else null if there was no valid client
for the message.

