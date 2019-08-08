[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [fetchAllDevicesAsync](./fetch-all-devices-async.md)

# fetchAllDevicesAsync

`fun fetchAllDevicesAsync(): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Device`](../../mozilla.components.concept.sync/-device/index.md)`>?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L103)

Overrides [DeviceConstellation.fetchAllDevicesAsync](../../mozilla.components.concept.sync/-device-constellation/fetch-all-devices-async.md)

Get all devices in the constellation.

**Return**
A list of all devices in the constellation, or `null` on failure.

