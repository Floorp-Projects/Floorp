[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [DeviceConfig](./index.md)

# DeviceConfig

`data class DeviceConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L27)

Configuration for the current device.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DeviceConfig(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)`, capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>)`<br>Configuration for the current device. |

### Properties

| Name | Summary |
|---|---|
| [capabilities](capabilities.md) | `val capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>`<br>A set of device capabilities, such as SEND_TAB. This set can be expanded by re-initializing [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) with a new set (e.g. on app restart). Shrinking a set of capabilities is currently not supported. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>An initial name to use for the device record which will be created during authentication. This can be changed later via [FxaDeviceConstellation](../-fxa-device-constellation/index.md). |
| [type](type.md) | `val type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)<br>Type of a device - mobile, desktop - used for displaying identifying icons on other devices. This cannot be changed once device record is created. |
