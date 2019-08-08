[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [setDeviceNameAsync](./set-device-name-async.md)

# setDeviceNameAsync

`fun setDeviceNameAsync(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L119)

Overrides [DeviceConstellation.setDeviceNameAsync](../../mozilla.components.concept.sync/-device-constellation/set-device-name-async.md)

Set name of the current device.

### Parameters

`name` - New device name.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

