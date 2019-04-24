[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [Storage](./index.md)

# Storage

`interface Storage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/Storage.kt#L10)

An interface which provides generic operations for storing browser data like history and bookmarks.

### Functions

| Name | Summary |
|---|---|
| [cleanup](cleanup.md) | `abstract fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [runMaintenance](run-maintenance.md) | `abstract suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs internal database maintenance tasks |

### Inheritors

| Name | Summary |
|---|---|
| [BookmarksStorage](../-bookmarks-storage/index.md) | `interface BookmarksStorage : `[`Storage`](./index.md)<br>An interface which defines read/write operations for bookmarks data. |
| [HistoryStorage](../-history-storage/index.md) | `interface HistoryStorage : `[`Storage`](./index.md)<br>An interface which defines read/write methods for history data. |
| [PlacesStorage](../../mozilla.components.browser.storage.sync/-places-storage/index.md) | `abstract class PlacesStorage : `[`Storage`](./index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>A base class for concrete implementations of PlacesStorages |
