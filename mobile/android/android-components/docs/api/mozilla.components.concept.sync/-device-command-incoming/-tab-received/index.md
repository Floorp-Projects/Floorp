[android-components](../../../index.md) / [mozilla.components.concept.sync](../../index.md) / [DeviceCommandIncoming](../index.md) / [TabReceived](./index.md)

# TabReceived

`class TabReceived : `[`DeviceCommandIncoming`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/AccountEvent.kt#L40)

A command to open a list of tabs on the current device

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabReceived(from: `[`Device`](../../-device/index.md)`?, entries: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabData`](../../-tab-data/index.md)`>)`<br>A command to open a list of tabs on the current device |

### Properties

| Name | Summary |
|---|---|
| [entries](entries.md) | `val entries: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabData`](../../-tab-data/index.md)`>` |
| [from](from.md) | `val from: `[`Device`](../../-device/index.md)`?` |
