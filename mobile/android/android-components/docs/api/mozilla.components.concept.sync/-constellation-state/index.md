[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [ConstellationState](./index.md)

# ConstellationState

`data class ConstellationState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Devices.kt#L101)

Describes current device and other devices in the constellation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ConstellationState(currentDevice: `[`Device`](../-device/index.md)`?, otherDevices: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Device`](../-device/index.md)`>)`<br>Describes current device and other devices in the constellation. |

### Properties

| Name | Summary |
|---|---|
| [currentDevice](current-device.md) | `val currentDevice: `[`Device`](../-device/index.md)`?` |
| [otherDevices](other-devices.md) | `val otherDevices: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Device`](../-device/index.md)`>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
