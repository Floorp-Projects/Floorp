[android-components](../index.md) / [mozilla.components.concept.push](./index.md)

## Package mozilla.components.concept.push

### Types

| Name | Summary |
|---|---|
| [EncryptedPushMessage](-encrypted-push-message/index.md) | `data class EncryptedPushMessage`<br>A push message holds the information needed to pass the message on to the appropriate receiver. |
| [PushProcessor](-push-processor/index.md) | `interface PushProcessor`<br>A push notification processor that handles registration and new messages from the [PushService](-push-service/index.md) provided. Starting Push in the Application's onCreate is recommended. |
| [PushService](-push-service/index.md) | `interface PushService`<br>Implemented by push services like Firebase Cloud Messaging and Amazon Device Messaging SDKs to allow the [PushProcessor](-push-processor/index.md) to manage their lifecycle. |

### Exceptions

| Name | Summary |
|---|---|
| [PushError](-push-error/index.md) | `sealed class PushError : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Various error types. |
