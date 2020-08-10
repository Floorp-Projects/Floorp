[android-components](../index.md) / [mozilla.components.feature.push](./index.md)

## Package mozilla.components.feature.push

### Types

| Name | Summary |
|---|---|
| [AutoPushFeature](-auto-push-feature/index.md) | `class AutoPushFeature : `[`PushProcessor`](../mozilla.components.concept.push/-push-processor/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-auto-push-feature/-observer/index.md)`>`<br>A implementation of a [PushProcessor](../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed in the Application's onCreate. It receives messages from a service and forwards them to be decrypted and routed. |
| [AutoPushSubscription](-auto-push-subscription/index.md) | `data class AutoPushSubscription`<br>The subscription information from AutoPush that can be used to send push messages to other devices. |
| [AutoPushSubscriptionChanged](-auto-push-subscription-changed/index.md) | `data class AutoPushSubscriptionChanged`<br>The subscription from AutoPush that has changed on the remote push servers. |
| [DecryptedMessage](-decrypted-message/index.md) | `data class DecryptedMessage`<br>Represents a decrypted push message for notifying observers of the [scope](-decrypted-message/scope.md). |
| [Protocol](-protocol/index.md) | `enum class Protocol`<br>Supported network protocols. |
| [PushConfig](-push-config/index.md) | `data class PushConfig`<br>Configuration object for initializing the Push Manager with an AutoPush server. |
| [PushConnection](-push-connection/index.md) | `interface PushConnection : `[`Closeable`](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html)<br>An interface that wraps the [PushAPI](#). |
| [ServiceType](-service-type/index.md) | `enum class ServiceType`<br>Supported push services. These are currently limited to Firebase Cloud Messaging and Amazon Device Messaging. |

### Type Aliases

| Name | Summary |
|---|---|
| [AppServerKey](-app-server-key.md) | `typealias AppServerKey = `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [PushScope](-push-scope.md) | `typealias PushScope = `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
