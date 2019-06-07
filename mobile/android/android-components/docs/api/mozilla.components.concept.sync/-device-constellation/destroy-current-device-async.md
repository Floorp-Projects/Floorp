[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [destroyCurrentDeviceAsync](./destroy-current-device-async.md)

# destroyCurrentDeviceAsync

`abstract fun destroyCurrentDeviceAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L36)

Destroy current device record.
Use this when device record is no longer relevant, e.g. while logging out. On success, other
devices will no longer see the current device in their device lists.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

