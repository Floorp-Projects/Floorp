[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [refreshDeviceStateAsync](./refresh-device-state-async.md)

# refreshDeviceStateAsync

`fun refreshDeviceStateAsync(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L166)

Overrides [DeviceConstellation.refreshDeviceStateAsync](../../mozilla.components.concept.sync/-device-constellation/refresh-device-state-async.md)

Refreshes [ConstellationState](../../mozilla.components.concept.sync/-constellation-state/index.md) and polls for device events.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete. Failure may
indicate a partial success.

