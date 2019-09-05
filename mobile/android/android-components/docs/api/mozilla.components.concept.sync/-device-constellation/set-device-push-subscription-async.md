[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [setDevicePushSubscriptionAsync](./set-device-push-subscription-async.md)

# setDevicePushSubscriptionAsync

`abstract fun setDevicePushSubscriptionAsync(subscription: `[`DevicePushSubscription`](../-device-push-subscription/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L63)

Set a [DevicePushSubscription](../-device-push-subscription/index.md) for the current device.

### Parameters

`subscription` - A new [DevicePushSubscription](../-device-push-subscription/index.md).

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

