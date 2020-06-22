[android-components](../index.md) / [mozilla.components.browser.storage.sync](./index.md)

## Package mozilla.components.browser.storage.sync

### Types

| Name | Summary |
|---|---|
| [PlacesBookmarksStorage](-places-bookmarks-storage/index.md) | `open class PlacesBookmarksStorage : `[`PlacesStorage`](-places-storage/index.md)`, `[`BookmarksStorage`](../mozilla.components.concept.storage/-bookmarks-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [BookmarksStorage](../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesHistoryStorage](-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`PlacesStorage`](-places-storage/index.md)`, `[`HistoryStorage`](../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [HistoryStorage](../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesStorage](-places-storage/index.md) | `abstract class PlacesStorage : `[`Storage`](../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>A base class for concrete implementations of PlacesStorages |
| [RemoteTabsStorage](-remote-tabs-storage/index.md) | `open class RemoteTabsStorage : `[`Storage`](../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>An interface which defines read/write methods for remote tabs data. |
| [SyncClient](-sync-client/index.md) | `data class SyncClient`<br>Represents a Sync client that can be associated with a list of opened tabs. |
| [SyncedDeviceTabs](-synced-device-tabs/index.md) | `data class SyncedDeviceTabs`<br>A synced device and its list of tabs. |
| [Tab](-tab/index.md) | `data class Tab`<br>A tab, which is defined by an history (think the previous/next button in your web browser) and a currently active history entry. |
| [TabEntry](-tab-entry/index.md) | `data class TabEntry`<br>A Tab history entry. |

### Properties

| Name | Summary |
|---|---|
| [AUTOCOMPLETE_SOURCE_NAME](-a-u-t-o-c-o-m-p-l-e-t-e_-s-o-u-r-c-e_-n-a-m-e.md) | `const val AUTOCOMPLETE_SOURCE_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DB_NAME](-d-b_-n-a-m-e.md) | `const val DB_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
