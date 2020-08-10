[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceCommandIncoming](./index.md)

# DeviceCommandIncoming

`sealed class DeviceCommandIncoming` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/AccountEvent.kt#L38)

Incoming device commands (ie, targeted at the current device.)

### Types

| Name | Summary |
|---|---|
| [TabReceived](-tab-received/index.md) | `class TabReceived : `[`DeviceCommandIncoming`](./index.md)<br>A command to open a list of tabs on the current device |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [TabReceived](-tab-received/index.md) | `class TabReceived : `[`DeviceCommandIncoming`](./index.md)<br>A command to open a list of tabs on the current device |
