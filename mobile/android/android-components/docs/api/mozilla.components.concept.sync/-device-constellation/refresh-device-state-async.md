[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [refreshDeviceStateAsync](./refresh-device-state-async.md)

# refreshDeviceStateAsync

`abstract fun refreshDeviceStateAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L109)

Refreshes [ConstellationState](../-constellation-state/index.md) and polls for device events.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete. Failure may
indicate a partial success.

