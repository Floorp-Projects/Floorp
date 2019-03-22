[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushService](./index.md)

# PushService

`interface PushService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushService.kt#L13)

Implemented by push services like Firebase Cloud Messaging and Amazon Device Messaging SDKs to allow
the [PushProcessor](../-push-processor/index.md) to manage their lifecycle.

### Properties

| Name | Summary |
|---|---|
| [instance](instance.md) | `abstract val instance: `[`PushService`](./index.md)<br>The instance of the running service. This can be useful for push SDKs that are started automatically and more control is needed. |

### Functions

| Name | Summary |
|---|---|
| [forceRegistrationRenewal](force-registration-renewal.md) | `abstract fun forceRegistrationRenewal(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Forces the push service to renew it's registration. This may lead to a new registration token being received. |
| [start](start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push service. |
| [stop](stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the push service. |
