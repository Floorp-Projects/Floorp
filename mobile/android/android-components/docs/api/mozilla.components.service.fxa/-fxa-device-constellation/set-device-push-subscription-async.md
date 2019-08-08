[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [setDevicePushSubscriptionAsync](./set-device-push-subscription-async.md)

# setDevicePushSubscriptionAsync

`fun setDevicePushSubscriptionAsync(subscription: `[`DevicePushSubscription`](../../mozilla.components.concept.sync/-device-push-subscription/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L131)

Overrides [DeviceConstellation.setDevicePushSubscriptionAsync](../../mozilla.components.concept.sync/-device-constellation/set-device-push-subscription-async.md)

Set a [DevicePushSubscription](../../mozilla.components.concept.sync/-device-push-subscription/index.md) for the current device.

### Parameters

`subscription` - A new [DevicePushSubscription](../../mozilla.components.concept.sync/-device-push-subscription/index.md).

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

