[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.storage](../index.md) / [SyncedTabsProvider](./index.md)

# SyncedTabsProvider

`interface SyncedTabsProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/storage/SyncedTabsProvider.kt#L12)

Provides tabs from remote Firefox Sync devices.

### Functions

| Name | Summary |
|---|---|
| [getSyncedDeviceTabs](get-synced-device-tabs.md) | `abstract suspend fun getSyncedDeviceTabs(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SyncedDeviceTabs`](../../mozilla.components.browser.storage.sync/-synced-device-tabs/index.md)`>`<br>A list of [SyncedDeviceTabs](../../mozilla.components.browser.storage.sync/-synced-device-tabs/index.md), each containing a synced device and its current tabs. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SyncedTabsStorage](../-synced-tabs-storage/index.md) | `class SyncedTabsStorage : `[`SyncedTabsProvider`](./index.md)<br>A storage that listens to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) changes to synchronize the local tabs state with [RemoteTabsStorage](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md). |
