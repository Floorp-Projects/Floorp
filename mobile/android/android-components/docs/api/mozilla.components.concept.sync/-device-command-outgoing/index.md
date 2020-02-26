[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [DeviceCommandOutgoing](./index.md)

# DeviceCommandOutgoing

`sealed class DeviceCommandOutgoing` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/AccountEvent.kt#L46)

Outgoing device commands (ie, targeted at other devices.)

### Types

| Name | Summary |
|---|---|
| [SendTab](-send-tab/index.md) | `class SendTab : `[`DeviceCommandOutgoing`](./index.md)<br>A command to open a tab on another device |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SendTab](-send-tab/index.md) | `class SendTab : `[`DeviceCommandOutgoing`](./index.md)<br>A command to open a tab on another device |
