[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [initDeviceAsync](./init-device-async.md)

# initDeviceAsync

`abstract fun initDeviceAsync(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`DeviceType`](../-device-type/index.md)` = DeviceType.MOBILE, capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L24)

Register current device in the associated [DeviceConstellation](index.md).

### Parameters

`name` - An initial name for the current device. This may be changed via [setDeviceNameAsync](set-device-name-async.md).

`type` - Type of the current device. This can't be changed.

`capabilities` - A list of capabilities that the current device claims to have.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

