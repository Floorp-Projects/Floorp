[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](./index.md)

# AutoPushFeature

`class AutoPushFeature : `[`PushProcessor`](../../mozilla.components.concept.push/-push-processor/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L67)

A implementation of a [PushProcessor](../../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed
in the Application's onCreate. It receives messages from a service and forwards them
to be decrypted and routed.

Listen for subscription information changes for each registered scope:

Listen also for push messages:

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Observers that want to receive updates for new subscriptions and messages. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoPushFeature(context: <ERROR CLASS>, service: `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md)`, config: `[`PushConfig`](../-push-config/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors.newSingleThreadExecutor().asCoroutineDispatcher(), connection: `[`PushConnection`](../-push-connection/index.md)` = RustPushConnection(
        senderId = config.senderId,
        serverHost = config.serverHost,
        socketProtocol = config.protocol,
        serviceType = config.serviceType,
        databasePath = File(context.filesDir, DB_NAME).canonicalPath
    ), crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null)`<br>A implementation of a [PushProcessor](../../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed in the Application's onCreate. It receives messages from a service and forwards them to be decrypted and routed. |

### Functions

| Name | Summary |
|---|---|
| [getSubscription](get-subscription.md) | `fun getSubscription(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, block: (`[`AutoPushSubscription`](../-auto-push-subscription/index.md)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Checks if a subscription for the [scope](get-subscription.md#mozilla.components.feature.push.AutoPushFeature$getSubscription(kotlin.String, kotlin.String, kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/scope) already exists. |
| [initialize](initialize.md) | `fun initialize(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the push feature and initialization work needed. Also starts the [PushService](../../mozilla.components.concept.push/-push-service/index.md) to ensure new messages come through. |
| [onError](on-error.md) | `fun onError(error: `[`PushError`](../../mozilla.components.concept.push/-push-error/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An error has occurred. |
| [onMessageReceived](on-message-received.md) | `fun onMessageReceived(message: `[`EncryptedPushMessage`](../../mozilla.components.concept.push/-encrypted-push-message/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>New encrypted messages received from a supported push messaging service. |
| [onNewToken](on-new-token.md) | `fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>New registration tokens are received and sent to the AutoPush server which also performs subscriptions for each push type and notifies the subscribers. |
| [renewRegistration](renew-registration.md) | `fun renewRegistration(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes the registration token locally so that it forces the service to get a new one the next time hits it's messaging server. |
| [shutdown](shutdown.md) | `fun shutdown(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Un-subscribes from all push message channels and stops periodic verifications. |
| [subscribe](subscribe.md) | `fun subscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appServerKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSubscribeError: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, onSubscribe: (`[`AutoPushSubscription`](../-auto-push-subscription/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Subscribes for push notifications and invokes the [onSubscribe](subscribe.md#mozilla.components.feature.push.AutoPushFeature$subscribe(kotlin.String, kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((mozilla.components.feature.push.AutoPushSubscription, kotlin.Unit)))/onSubscribe) callback with the subscription information. |
| [unsubscribe](unsubscribe.md) | `fun unsubscribe(scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, onUnsubscribeError: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, onUnsubscribe: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Un-subscribes from a valid subscription and invokes the [onUnsubscribe](unsubscribe.md#mozilla.components.feature.push.AutoPushFeature$unsubscribe(kotlin.String, kotlin.Function0((kotlin.Unit)), kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/onUnsubscribe) callback with the result. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
