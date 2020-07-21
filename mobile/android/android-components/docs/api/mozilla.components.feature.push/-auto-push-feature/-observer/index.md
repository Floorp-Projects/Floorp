[android-components](../../../index.md) / [mozilla.components.feature.push](../../index.md) / [AutoPushFeature](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L329)

Observers that want to receive updates for new subscriptions and messages.

### Functions

| Name | Summary |
|---|---|
| [onMessageReceived](on-message-received.md) | `open fun onMessageReceived(scope: `[`PushScope`](../../-push-scope.md)`, message: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A messaged has been received for the [scope](on-message-received.md#mozilla.components.feature.push.AutoPushFeature.Observer$onMessageReceived(kotlin.String, kotlin.ByteArray)/scope). |
| [onSubscriptionChanged](on-subscription-changed.md) | `open fun onSubscriptionChanged(scope: `[`PushScope`](../../-push-scope.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A subscription for the scope is available. |
