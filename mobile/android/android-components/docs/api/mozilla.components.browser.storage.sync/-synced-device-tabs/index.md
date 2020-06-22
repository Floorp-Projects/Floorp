[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [SyncedDeviceTabs](./index.md)

# SyncedDeviceTabs

`data class SyncedDeviceTabs` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/RemoteTabsStorage.kt#L152)

A synced device and its list of tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncedDeviceTabs(device: `[`Device`](../../mozilla.components.concept.sync/-device/index.md)`, tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>)`<br>A synced device and its list of tabs. |

### Properties

| Name | Summary |
|---|---|
| [device](device.md) | `val device: `[`Device`](../../mozilla.components.concept.sync/-device/index.md) |
| [tabs](tabs.md) | `val tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
