[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConnection](./index.md)

# PushConnection

`interface PushConnection : `[`Closeable`](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/Connection.kt#L28)

An interface that wraps the [PushAPI](#).

This aides in testing and abstracting out the hurdles of initialization checks required before performing actions
on the API.

### Functions

| Name | Summary |
|---|---|
| [containsSubscription](contains-subscription.md) | `abstract suspend fun containsSubscription(scope: `[`PushScope`](../-push-scope.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns `true` if the specified [scope](contains-subscription.md#mozilla.components.feature.push.PushConnection$containsSubscription(kotlin.String)/scope) has a subscription. |
| [decryptMessage](decrypt-message.md) | `abstract suspend fun decryptMessage(channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", salt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", cryptoKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = ""): `[`DecryptedMessage`](../-decrypted-message/index.md)`?`<br>Decrypts a received message. |
| [isInitialized](is-initialized.md) | `abstract fun isInitialized(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the native Push API has already been initialized. |
| [subscribe](subscribe.md) | `abstract suspend fun subscribe(scope: `[`PushScope`](../-push-scope.md)`, appServerKey: `[`AppServerKey`](../-app-server-key.md)`? = null): `[`AutoPushSubscription`](../-auto-push-subscription/index.md)<br>Creates a push subscription for the given scope. |
| [unsubscribe](unsubscribe.md) | `abstract suspend fun unsubscribe(scope: `[`PushScope`](../-push-scope.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Un-subscribes a push subscription for the given scope. |
| [unsubscribeAll](unsubscribe-all.md) | `abstract suspend fun unsubscribeAll(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Un-subscribes all push subscriptions. |
| [updateToken](update-token.md) | `abstract suspend fun updateToken(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Updates the registration token to the native Push API if it changes. |
| [verifyConnection](verify-connection.md) | `abstract suspend fun verifyConnection(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AutoPushSubscriptionChanged`](../-auto-push-subscription-changed/index.md)`>`<br>Checks validity of current push subscriptions. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
