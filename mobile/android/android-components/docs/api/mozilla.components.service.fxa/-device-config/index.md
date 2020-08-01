[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [DeviceConfig](./index.md)

# DeviceConfig

`data class DeviceConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L37)

Configuration for the current device.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DeviceConfig(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)`, capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>, secureStateAtRest: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Configuration for the current device. |

### Properties

| Name | Summary |
|---|---|
| [capabilities](capabilities.md) | `val capabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`DeviceCapability`](../../mozilla.components.concept.sync/-device-capability/index.md)`>`<br>A set of device capabilities, such as SEND_TAB. This set can be expanded by re-initializing [FxaAccountManager](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) with a new set (e.g. on app restart). Shrinking a set of capabilities is currently not supported. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>An initial name to use for the device record which will be created during authentication. This can be changed later via [FxaDeviceConstellation](../-fxa-device-constellation/index.md). |
| [secureStateAtRest](secure-state-at-rest.md) | `val secureStateAtRest: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>A flag indicating whether or not to use encrypted storage for the persisted account state. If set to `true`, [SecureAbove22AccountStorage](#) will be used as a storage layer. As the name suggests, account state will only by encrypted on Android API 23+. Otherwise, even if this flag is set to `true`, account state will be stored in plaintext. |
| [type](type.md) | `val type: `[`DeviceType`](../../mozilla.components.concept.sync/-device-type/index.md)<br>Type of a device - mobile, desktop - used for displaying identifying icons on other devices. This cannot be changed once device record is created. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
