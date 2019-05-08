[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [ensureCapabilitiesAsync](./ensure-capabilities-async.md)

# ensureCapabilitiesAsync

`fun ensureCapabilitiesAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L62)

Overrides [DeviceConstellation.ensureCapabilitiesAsync](../../mozilla.components.concept.sync/-device-constellation/ensure-capabilities-async.md)

Ensure that all initialized [DeviceCapability](../../mozilla.components.concept.sync/-device-capability/index.md), such as [DeviceCapability.SEND_TAB](../../mozilla.components.concept.sync/-device-capability/-s-e-n-d_-t-a-b.md), are configured.
This may involve backend service registration, or other work involving network/disc access.

**Return**
A [Deferred](#) that will be resolved once operation is complete.

