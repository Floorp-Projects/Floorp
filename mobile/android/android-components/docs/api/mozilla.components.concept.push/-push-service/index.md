[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushService](./index.md)

# PushService

`interface PushService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushService.kt#L13)

Implemented by push services like Firebase Cloud Messaging and Amazon Device Messaging SDKs to allow
the [PushProcessor](../-push-processor/index.md) to manage their lifecycle.

### Functions

| Name | Summary |
|---|---|
| [deleteToken](delete-token.md) | `abstract fun deleteToken(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tells the push service to delete the registration token. |
| [isServiceAvailable](is-service-available.md) | `abstract fun isServiceAvailable(context: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the push service is support on the device. |
| [start](start.md) | `abstract fun start(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push service. |
| [stop](stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the push service. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [MESSAGE_KEY_BODY](-m-e-s-s-a-g-e_-k-e-y_-b-o-d-y.md) | `const val MESSAGE_KEY_BODY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message key for the body in a push message. |
| [MESSAGE_KEY_CHANNEL_ID](-m-e-s-s-a-g-e_-k-e-y_-c-h-a-n-n-e-l_-i-d.md) | `const val MESSAGE_KEY_CHANNEL_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message key for "channel ID" in a push message. |
| [MESSAGE_KEY_CRYPTO_KEY](-m-e-s-s-a-g-e_-k-e-y_-c-r-y-p-t-o_-k-e-y.md) | `const val MESSAGE_KEY_CRYPTO_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message key for "cryptoKey" in a push message. |
| [MESSAGE_KEY_ENCODING](-m-e-s-s-a-g-e_-k-e-y_-e-n-c-o-d-i-n-g.md) | `const val MESSAGE_KEY_ENCODING: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message key for encoding in a push message. |
| [MESSAGE_KEY_SALT](-m-e-s-s-a-g-e_-k-e-y_-s-a-l-t.md) | `const val MESSAGE_KEY_SALT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Message key for encryption salt in a push message. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AbstractAmazonPushService](../../mozilla.components.lib.push.amazon/-abstract-amazon-push-service/index.md) | `abstract class AbstractAmazonPushService : ADMMessageHandlerBase, `[`PushService`](./index.md)<br>An Amazon Cloud Messaging implementation of the [PushService](./index.md) for Android devices that support Google Play Services. [ADMMessageHandlerBase](#) requires a redundant constructor parameter. |
| [AbstractFirebasePushService](../../mozilla.components.lib.push.firebase/-abstract-firebase-push-service/index.md) | `abstract class AbstractFirebasePushService : FirebaseMessagingService, `[`PushService`](./index.md)<br>A Firebase Cloud Messaging implementation of the [PushService](./index.md) for Android devices that support Google Play Services. |
