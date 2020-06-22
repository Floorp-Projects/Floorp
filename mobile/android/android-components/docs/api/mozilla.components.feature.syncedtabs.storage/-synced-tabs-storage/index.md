[android-components](../../index.md) / [mozilla.components.feature.syncedtabs.storage](../index.md) / [SyncedTabsStorage](./index.md)

# SyncedTabsStorage

`class SyncedTabsStorage : `[`SyncedTabsProvider`](../-synced-tabs-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/storage/SyncedTabsStorage.kt#L27)

A storage that listens to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) changes to synchronize the local tabs state
with [RemoteTabsStorage](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncedTabsStorage(accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, tabsStorage: `[`RemoteTabsStorage`](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md)` = RemoteTabsStorage())`<br>A storage that listens to the [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md) changes to synchronize the local tabs state with [RemoteTabsStorage](../../mozilla.components.browser.storage.sync/-remote-tabs-storage/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getSyncedDeviceTabs](get-synced-device-tabs.md) | `suspend fun getSyncedDeviceTabs(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SyncedDeviceTabs`](../../mozilla.components.browser.storage.sync/-synced-device-tabs/index.md)`>`<br>See [SyncedTabsProvider.getSyncedDeviceTabs](../-synced-tabs-provider/get-synced-device-tabs.md). |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start listening to browser store changes. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop listening to browser store changes. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
