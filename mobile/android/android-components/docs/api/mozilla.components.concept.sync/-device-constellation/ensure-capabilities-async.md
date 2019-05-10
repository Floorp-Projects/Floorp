[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceConstellation](index.md) / [ensureCapabilitiesAsync](./ensure-capabilities-async.md)

# ensureCapabilitiesAsync

`abstract fun ensureCapabilitiesAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L43)

Ensure that all initialized [DeviceCapability](../-device-capability/index.md), such as [DeviceCapability.SEND_TAB](../-device-capability/-s-e-n-d_-t-a-b.md), are configured.
This may involve backend service registration, or other work involving network/disc access.

**Return**
A [Deferred](#) that will be resolved once operation is complete.

