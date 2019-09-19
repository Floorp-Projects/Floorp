[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [refreshDevicesAsync](./refresh-devices-async.md)

# refreshDevicesAsync

`abstract fun refreshDevicesAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L85)

Refreshes [ConstellationState](../-constellation-state/index.md). Registered [DeviceConstellationObserver](../-device-constellation-observer/index.md) observers will be notified.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

