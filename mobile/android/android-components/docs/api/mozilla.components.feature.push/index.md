[android-components](../index.md) / [mozilla.components.feature.push](./index.md)

## Package mozilla.components.feature.push

### Types

| Name | Summary |
|---|---|
| [AutoPushFeature](-auto-push-feature/index.md) | `class AutoPushFeature : `[`PushProcessor`](../mozilla.components.concept.push/-push-processor/index.md)<br>A implementation of a [PushProcessor](../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed in the Application's onCreate. It receives messages from a service and forwards them to be decrypted and routed. |
| [AutoPushSubscription](-auto-push-subscription/index.md) | `data class AutoPushSubscription`<br>The subscription information from AutoPush that can be used to send push messages to other devices. |
| [MessageBus](-message-bus/index.md) | `class MessageBus<T : `[`Enum`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-enum/index.html)`<`[`T`](-message-bus/index.md#T)`>, M> : `[`Bus`](../mozilla.components.concept.push/-bus/index.md)`<`[`T`](-message-bus/index.md#T)`, `[`M`](-message-bus/index.md#M)`>`<br>An implementation of [Bus](../mozilla.components.concept.push/-bus/index.md) where the event type is restricted to an enum. |
| [Protocol](-protocol/index.md) | `enum class Protocol`<br>Supported network protocols. |
| [PushConfig](-push-config/index.md) | `data class PushConfig`<br>Configuration object for initializing the Push Manager with an AutoPush server. |
| [PushConnection](-push-connection/index.md) | `interface PushConnection : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)<br>An interface that wraps the [PushAPI](#). |
| [PushSubscriptionObserver](-push-subscription-observer/index.md) | `interface PushSubscriptionObserver`<br>Observers that want to receive updates for new subscriptions. |
| [PushType](-push-type/index.md) | `enum class PushType`<br>The different kind of message types that a [EncryptedPushMessage](../mozilla.components.concept.push/-encrypted-push-message/index.md) can be: |
| [ServiceType](-service-type/index.md) | `enum class ServiceType`<br>Supported push services. These are currently limited to Firebase Cloud Messaging and Amazon Device Messaging. |

### Type Aliases

| Name | Summary |
|---|---|
| [Registry](-registry.md) | `typealias Registry<T, M> = `[`ObserverRegistry`](../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`Observer`](../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`T`](-registry.md#T)`, `[`M`](-registry.md#M)`>>` |
