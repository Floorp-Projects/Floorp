[android-components](../../index.md) / [mozilla.components.concept.push](../index.md) / [PushProcessor](./index.md)

# PushProcessor

`abstract class PushProcessor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/push/src/main/java/mozilla/components/concept/push/PushProcessor.kt#L15)

A push notification processor that handles registration and new messages from the [PushService](../-push-service/index.md) provided.
Starting Push in the Application's onCreate is recommended.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PushProcessor()`<br>A push notification processor that handles registration and new messages from the [PushService](../-push-service/index.md) provided. Starting Push in the Application's onCreate is recommended. |

### Functions

| Name | Summary |
|---|---|
| [initialize](initialize.md) | `open fun initialize(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Initialize the PushProcessor before it |
| [onError](on-error.md) | `abstract fun onError(error: `[`Error`](../-error/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An error has occurred. |
| [onMessageReceived](on-message-received.md) | `abstract fun onMessageReceived(message: `[`PushMessage`](../-push-message/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new push message has been received. |
| [onNewToken](on-new-token.md) | `abstract fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new registration token has been received. |
| [start](start.md) | `abstract fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start the push processor and starts any service associated. |
| [stop](stop.md) | `abstract fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop the push service and stops any service associated. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [instance](instance.md) | `var instance: `[`PushProcessor`](./index.md)`?` |
| [requireInstance](require-instance.md) | `val requireInstance: `[`PushProcessor`](./index.md) |
