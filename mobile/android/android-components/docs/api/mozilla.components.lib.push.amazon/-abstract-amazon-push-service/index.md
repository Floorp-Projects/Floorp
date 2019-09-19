[android-components](../../index.md) / [mozilla.components.lib.push.amazon](../index.md) / [AbstractAmazonPushService](./index.md)

# AbstractAmazonPushService

`abstract class AbstractAmazonPushService : ADMMessageHandlerBase, `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/push-amazon/src/main/java/mozilla/components/lib/push/amazon/AbstractAmazonPushService.kt#L25)

An Amazon Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services.
[ADMMessageHandlerBase](#) requires a redundant constructor parameter.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractAmazonPushService()`<br>An Amazon Cloud Messaging implementation of the [PushService](../../mozilla.components.concept.push/-push-service/index.md) for Android devices that support Google Play Services. [ADMMessageHandlerBase](#) requires a redundant constructor parameter. |

### Functions

| Name | Summary |
|---|---|
| [deleteToken](delete-token.md) | `open fun deleteToken(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tells the push service to delete the registration token. |
| [isServiceAvailable](is-service-available.md) | `open fun isServiceAvailable(context: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the push service is support on the device. |
| [onMessage](on-message.md) | `open fun onMessage(intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onRegistered](on-registered.md) | `open fun onRegistered(registrationId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onRegistrationError](on-registration-error.md) | `open fun onRegistrationError(errorId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onUnregistered](on-unregistered.md) | `open fun onUnregistered(registrationId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `open fun start(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push service. |
| [stop](stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the push service. |
