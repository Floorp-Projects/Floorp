[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](./index.md)

# AutoPushFeature

`class AutoPushFeature : `[`PushProcessor`](../../mozilla.components.concept.push/-push-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L65)

A implementation of a [PushProcessor](../../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed
in the Application's onCreate. It receives messages from a service and forwards them
to be decrypted and routed.

Listen for subscription information changes for each registered [PushType](../-push-type/index.md):

Listen also for push messages for each registered [PushType](../-push-type/index.md):

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoPushFeature(context: <ERROR CLASS>, service: `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md)`, config: `[`PushConfig`](../-push-config/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors.newSingleThreadExecutor().asCoroutineDispatcher(), connection: `[`PushConnection`](../-push-connection/index.md)` = RustPushConnection(
        senderId = config.senderId,
        serverHost = config.serverHost,
        socketProtocol = config.protocol,
        serviceType = config.serviceType,
        databasePath = File(context.filesDir, DB_NAME).canonicalPath
    ))`<br>A implementation of a [PushProcessor](../../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed in the Application's onCreate. It receives messages from a service and forwards them to be decrypted and routed. |

### Functions

| Name | Summary |
|---|---|
| [forceRegistrationRenewal](force-registration-renewal.md) | `fun forceRegistrationRenewal(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes the registration token locally so that it forces the service to get a new one the next time hits it's messaging server. |
| [initialize](initialize.md) | `fun initialize(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push service provided. |
| [onError](on-error.md) | `fun onError(error: `[`PushError`](../../mozilla.components.concept.push/-push-error/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An error has occurred. |
| [onMessageReceived](on-message-received.md) | `fun onMessageReceived(message: `[`EncryptedPushMessage`](../../mozilla.components.concept.push/-encrypted-push-message/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>New encrypted messages received from a supported push messaging service. |
| [onNewToken](on-new-token.md) | `fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>New registration tokens are received and sent to the Autopush server which also performs subscriptions for each push type and notifies the subscribers. |
| [registerForPushMessages](register-for-push-messages.md) | `fun registerForPushMessages(type: `[`PushType`](../-push-type/index.md)`, observer: `[`Observer`](../../mozilla.components.concept.push/-bus/-observer/index.md)`<`[`PushType`](../-push-type/index.md)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Register to receive push messages for the associated [PushType](../-push-type/index.md). |
| [registerForSubscriptions](register-for-subscriptions.md) | `fun registerForSubscriptions(observer: `[`PushSubscriptionObserver`](../-push-subscription-observer/index.md)`, owner: LifecycleOwner = ProcessLifecycleOwner.get(), autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Register to receive push subscriptions when requested or when they have been re-registered. |
| [shutdown](shutdown.md) | `fun shutdown(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Un-subscribes from all push message channels and stops the push service. This should only be done on an account logout or app data deletion. |
| [subscribeAll](subscribe-all.md) | `fun subscribeAll(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Returns all subscription for the push type if available. |
| [subscribeForType](subscribe-for-type.md) | `fun subscribeForType(type: `[`PushType`](../-push-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies observers about the subscription information for the push type if available. |
| [unsubscribeForType](unsubscribe-for-type.md) | `fun unsubscribeForType(type: `[`PushType`](../-push-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Returns subscription information for the push type if available. |
