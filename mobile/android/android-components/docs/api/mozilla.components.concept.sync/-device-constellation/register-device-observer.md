[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [registerDeviceObserver](./register-device-observer.md)

# registerDeviceObserver

`abstract fun registerDeviceObserver(observer: `[`DeviceConstellationObserver`](../-device-constellation-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L49)

Allows monitoring state of the device constellation via [DeviceConstellationObserver](../-device-constellation-observer/index.md).
Use this to be notified of changes to the current device or other devices.

