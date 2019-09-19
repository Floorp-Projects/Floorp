[android-components](../../index.md) / [mozilla.components.lib.push.firebase](../index.md) / [AbstractFirebasePushService](./index.md)

# AbstractFirebasePushService

`abstract class AbstractFirebasePushService : FirebaseMessagingService, `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/push-firebase/src/main/java/mozilla/components/lib/push/firebase/AbstractFirebasePushService.kt#L31)

A Firebase Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractFirebasePushService(coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>A Firebase Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services. |

### Functions

| Name | Summary |
|---|---|
| [deleteToken](delete-token.md) | `open fun deleteToken(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the Firebase instance ID. This would lead a new token being generated when the service hits the Firebase servers. |
| [isServiceAvailable](is-service-available.md) | `open fun isServiceAvailable(context: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the push service is support on the device. |
| [onMessageReceived](on-message-received.md) | `open fun onMessageReceived(remoteMessage: RemoteMessage?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onNewToken](on-new-token.md) | `open fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `open fun start(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initializes Firebase and starts the messaging service if not already started and enables auto-start as well. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the Firebase messaging service and disables auto-start. |
