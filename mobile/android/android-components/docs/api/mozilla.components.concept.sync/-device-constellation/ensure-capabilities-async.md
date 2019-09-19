[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [ensureCapabilitiesAsync](./ensure-capabilities-async.md)

# ensureCapabilitiesAsync

`abstract fun ensureCapabilitiesAsync(capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../-device-capability/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L37)

Ensure that all passed in [capabilities](ensure-capabilities-async.md#mozilla.components.concept.sync.DeviceConstellation$ensureCapabilitiesAsync(kotlin.collections.Set((mozilla.components.concept.sync.DeviceCapability)))/capabilities) are configured.
This may involve backend service registration, or other work involving network/disc access.

### Parameters

`capabilities` - A list of capabilities to configure. This is expected to be the same or
longer list than what was passed into [initDeviceAsync](init-device-async.md). Removing capabilities is currently
not supported.

**Return**
A [Deferred](#) that will be resolved with a success flag once operation is complete.

