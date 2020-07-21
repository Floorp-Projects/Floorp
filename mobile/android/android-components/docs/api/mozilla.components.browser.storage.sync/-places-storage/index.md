[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesStorage](./index.md)

# PlacesStorage

`abstract class PlacesStorage : `[`Storage`](../../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesStorage.kt#L27)

A base class for concrete implementations of PlacesStorages

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PlacesStorage(context: <ERROR CLASS>)`<br>A base class for concrete implementations of PlacesStorages |

### Properties

| Name | Summary |
|---|---|
| [logger](logger.md) | `abstract val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md) |

### Functions

| Name | Summary |
|---|---|
| [cleanup](cleanup.md) | `open fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [ignoreUrlExceptions](ignore-url-exceptions.md) | `fun ignoreUrlExceptions(operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs [block](ignore-url-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$ignoreUrlExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/block) described by [operation](ignore-url-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$ignoreUrlExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/operation), ignoring and logging any thrown [UrlParseFailed](#) exceptions. |
| [runMaintenance](run-maintenance.md) | `open suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Internal database maintenance tasks. Ideally this should be called once a day. |
| [syncAndHandleExceptions](sync-and-handle-exceptions.md) | `fun syncAndHandleExceptions(syncBlock: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Runs a [syncBlock](sync-and-handle-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$syncAndHandleExceptions(kotlin.Function0((kotlin.Unit)))/syncBlock), re-throwing any panics that may be encountered. |
| [warmUp](warm-up.md) | `open suspend fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Make sure underlying database connections are established. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getHandle](../../mozilla.components.concept.sync/-syncable-store/get-handle.md) | `abstract fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [PlacesBookmarksStorage](../-places-bookmarks-storage/index.md) | `open class PlacesBookmarksStorage : `[`PlacesStorage`](./index.md)`, `[`BookmarksStorage`](../../mozilla.components.concept.storage/-bookmarks-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [BookmarksStorage](../../mozilla.components.concept.storage/-bookmarks-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
| [PlacesHistoryStorage](../-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`PlacesStorage`](./index.md)`, `[`HistoryStorage`](../../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [HistoryStorage](../../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesApi](#). |
