[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushSubscriptionObserver](./index.md)

# PushSubscriptionObserver

`interface PushSubscriptionObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L360)

Observers that want to receive updates for new subscriptions.

### Functions

| Name | Summary |
|---|---|
| [onSubscriptionAvailable](on-subscription-available.md) | `abstract fun onSubscriptionAvailable(subscription: `[`AutoPushSubscription`](../-auto-push-subscription/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
