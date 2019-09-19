[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceConstellation](index.md) / [ensureCapabilitiesAsync](./ensure-capabilities-async.md)

# ensureCapabilitiesAsync

`fun ensureCapabilitiesAsync(capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceConstellation.kt#L60)

Overrides [DeviceConstellation.ensureCapabilitiesAsync](../../mozilla.components.concept.sync/-device-constellation/ensure-capabilities-async.md)

Ensure that all passed in [capabilities](../../mozilla.components.concept.sync/-device-constellation/ensure-capabilities-async.md#mozilla.components.concept.sync.DeviceConstellation$ensureCapabilitiesAsync(kotlin.collections.Set((mozilla.components.concept.sync.DeviceCapability)))/capabilities) are configured.
This may involve backend service registration, or other work involving network/disc access.

### Parameters

`capabilities` - A list of capabilities to configure. This is expected to be the same or
longer list than what was passed into [initDeviceAsync](../../mozilla.components.concept.sync/-device-constellation/init-device-async.md). Removing capabilities is currently
not supported.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

