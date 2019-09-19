[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [registerDeviceObserver](./register-device-observer.md)

# registerDeviceObserver

`fun registerDeviceObserver(observer: `[`DeviceConstellationObserver`](../../mozilla.components.concept.sync/-device-constellation-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L76)

Overrides [DeviceConstellation.registerDeviceObserver](../../mozilla.components.concept.sync/-device-constellation/register-device-observer.md)

Allows monitoring state of the device constellation via [DeviceConstellationObserver](../../mozilla.components.concept.sync/-device-constellation-observer/index.md).
Use this to be notified of changes to the current device or other devices.

