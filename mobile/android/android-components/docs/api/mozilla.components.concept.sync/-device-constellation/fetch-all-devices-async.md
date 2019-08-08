[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [fetchAllDevicesAsync](./fetch-all-devices-async.md)

# fetchAllDevicesAsync

`abstract fun fetchAllDevicesAsync(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Device`](../-device/index.md)`>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L55)

Get all devices in the constellation.

**Return**
A list of all devices in the constellation, or `null` on failure.

