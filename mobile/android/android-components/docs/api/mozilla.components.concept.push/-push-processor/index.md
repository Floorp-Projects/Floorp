[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushProcessor](./index.md)

# PushProcessor

`interface PushProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushProcessor.kt#L13)

A push notification processor that handles registration and new messages from the [PushService](../-push-service/index.md) provided.
Starting Push in the Application's onCreate is recommended.

### Functions

| Name | Summary |
|---|---|
| [initialize](initialize.md) | `abstract fun initialize(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start the push processor and any service associated. |
| [onError](on-error.md) | `abstract fun onError(error: `[`PushError`](../-push-error/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An error has occurred. |
| [onMessageReceived](on-message-received.md) | `abstract fun onMessageReceived(message: `[`EncryptedPushMessage`](../-encrypted-push-message/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new push message has been received. |
| [onNewToken](on-new-token.md) | `abstract fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new registration token has been received. |
| [renewRegistration](renew-registration.md) | `abstract fun renewRegistration(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests the [PushService](../-push-service/index.md) to renew it's registration with it's provider. |
| [shutdown](shutdown.md) | `abstract fun shutdown(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all push subscriptions from the device. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [requireInstance](require-instance.md) | `val requireInstance: `[`PushProcessor`](./index.md) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [install](install.md) | `fun install(processor: `[`PushProcessor`](./index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize and installs the PushProcessor into the application. This needs to be called in the application's onCreate before a push service has started. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AutoPushFeature](../../mozilla.components.feature.push/-auto-push-feature/index.md) | `class AutoPushFeature : `[`PushProcessor`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.feature.push/-auto-push-feature/-observer/index.md)`>`<br>A implementation of a [PushProcessor](./index.md) that should live as a singleton by being installed in the Application's onCreate. It receives messages from a service and forwards them to be decrypted and routed. |
