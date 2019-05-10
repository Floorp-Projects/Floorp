[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [destroyCurrentDeviceAsync](./destroy-current-device-async.md)

# destroyCurrentDeviceAsync

`fun destroyCurrentDeviceAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L63)

Overrides [DeviceConstellation.destroyCurrentDeviceAsync](../../mozilla.components.concept.sync/-device-constellation/destroy-current-device-async.md)

Destroy current device record.
Use this when device record is no longer relevant, e.g. while logging out. On success, other
devices will no longer see the current device in their device lists.

**Return**
A boolean flag indicating if the operation succeeded.

