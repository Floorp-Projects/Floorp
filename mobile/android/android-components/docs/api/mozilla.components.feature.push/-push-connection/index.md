[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConnection](./index.md)

# PushConnection

`interface PushConnection : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/Connection.kt#L21)

An interface that wraps the [PushAPI](#).

This aides in testing and abstracting out the hurdles of initialization checks required before performing actions
on the API.

### Functions

| Name | Summary |
|---|---|
| [decrypt](decrypt.md) | `abstract fun decrypt(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) |
| [isInitialized](is-initialized.md) | `abstract fun isInitialized(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [subscribe](subscribe.md) | `abstract suspend fun subscribe(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): SubscriptionResponse` |
| [unsubscribe](unsubscribe.md) | `abstract suspend fun unsubscribe(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [unsubscribeAll](unsubscribe-all.md) | `abstract suspend fun unsubscribeAll(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [updateToken](update-token.md) | `abstract suspend fun updateToken(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [verifyConnection](verify-connection.md) | `abstract suspend fun verifyConnection(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
