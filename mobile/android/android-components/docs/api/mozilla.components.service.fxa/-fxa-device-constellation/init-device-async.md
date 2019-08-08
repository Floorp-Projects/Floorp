[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [initDeviceAsync](./init-device-async.md)

# initDeviceAsync

`fun initDeviceAsync(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)`, capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L57)

Overrides [DeviceConstellation.initDeviceAsync](../../mozilla.components.concept.sync/-device-constellation/init-device-async.md)

Register current device in the associated [DeviceConstellation](../../mozilla.components.concept.sync/-device-constellation/index.md).

### Parameters

`name` - An initial name for the current device. This may be changed via [setDeviceNameAsync](../../mozilla.components.concept.sync/-device-constellation/set-device-name-async.md).

`type` - Type of the current device. This can't be changed.

`capabilities` - A list of capabilities that the current device claims to have.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

