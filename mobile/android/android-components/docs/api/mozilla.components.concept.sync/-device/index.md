[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [Device](./index.md)

# Device

`data class Device` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L155)

Describes a device in the [DeviceConstellation](../-device-constellation/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Device(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, displayName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, deviceType: `[`DeviceType`](../-device-type/index.md)`, isCurrentDevice: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, lastAccessTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?, capabilities: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`DeviceCapability`](../-device-capability/index.md)`>, subscriptionExpired: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, subscription: `[`DevicePushSubscription`](../-device-push-subscription/index.md)`?)`<br>Describes a device in the [DeviceConstellation](../-device-constellation/index.md). |

### Properties

| Name | Summary |
|---|---|
| [capabilities](capabilities.md) | `val capabilities: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`DeviceCapability`](../-device-capability/index.md)`>` |
| [deviceType](device-type.md) | `val deviceType: `[`DeviceType`](../-device-type/index.md) |
| [displayName](display-name.md) | `val displayName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [isCurrentDevice](is-current-device.md) | `val isCurrentDevice: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [lastAccessTime](last-access-time.md) | `val lastAccessTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?` |
| [subscription](subscription.md) | `val subscription: `[`DevicePushSubscription`](../-device-push-subscription/index.md)`?` |
| [subscriptionExpired](subscription-expired.md) | `val subscriptionExpired: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [into](../../mozilla.components.service.fxa/into.md) | `fun `[`Device`](./index.md)`.into(): Device` |
