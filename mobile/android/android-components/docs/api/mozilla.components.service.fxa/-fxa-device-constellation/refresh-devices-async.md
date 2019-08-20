[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [refreshDevicesAsync](./refresh-devices-async.md)

# refreshDevicesAsync

`fun refreshDevicesAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L138)

Overrides [DeviceConstellation.refreshDevicesAsync](../../mozilla.components.concept.sync/-device-constellation/refresh-devices-async.md)

Refreshes [ConstellationState](../../mozilla.components.concept.sync/-constellation-state/index.md). Registered [DeviceConstellationObserver](../../mozilla.components.concept.sync/-device-constellation-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

