[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushService](./index.md)

# PushService

`interface PushService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushService.kt#L15)

Implemented by push services like Firebase Cloud Messaging and Amazon Device Messaging SDKs to allow
the [PushProcessor](../-push-processor/index.md) to manage their lifecycle.

### Functions

| Name | Summary |
|---|---|
| [deleteToken](delete-token.md) | `abstract fun deleteToken(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tells the push service to delete the registration token. |
| [isServiceAvailable](is-service-available.md) | `abstract fun isServiceAvailable(context: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the push service is support on the device. |
| [start](start.md) | `abstract fun start(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push service. |
| [stop](stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the push service. |

### Inheritors

| Name | Summary |
|---|---|
| [AbstractAmazonPushService](../../mozilla.components.lib.push.amazon/-abstract-amazon-push-service/index.md) | `abstract class AbstractAmazonPushService : ADMMessageHandlerBase, `[`PushService`](./index.md)<br>An Amazon Cloud Messaging implementation of the [PushService](./index.md) for Android devices that support Google Play Services. [ADMMessageHandlerBase](#) requires a redundant constructor parameter. |
| [AbstractFirebasePushService](../../mozilla.components.lib.push.firebase/-abstract-firebase-push-service/index.md) | `abstract class AbstractFirebasePushService : FirebaseMessagingService, `[`PushService`](./index.md)<br>A Firebase Cloud Messaging implementation of the [PushService](./index.md) for Android devices that support Google Play Services. |
