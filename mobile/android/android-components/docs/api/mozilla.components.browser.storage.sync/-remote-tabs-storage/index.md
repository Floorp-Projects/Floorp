[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [RemoteTabsStorage](./index.md)

# RemoteTabsStorage

`open class RemoteTabsStorage : `[`Storage`](../../mozilla.components.concept.storage/-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/RemoteTabsStorage.kt#L26)

An interface which defines read/write methods for remote tabs data.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RemoteTabsStorage()`<br>An interface which defines read/write methods for remote tabs data. |

### Functions

| Name | Summary |
|---|---|
| [cleanup](cleanup.md) | `open fun cleanup(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Cleans up background work and database connections |
| [getAll](get-all.md) | `suspend fun getAll(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`SyncClient`](../-sync-client/index.md)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>>`<br>Get all remote devices tabs. |
| [getHandle](get-handle.md) | `open fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [runMaintenance](run-maintenance.md) | `open suspend fun runMaintenance(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Runs internal database maintenance tasks |
| [store](store.md) | `suspend fun store(tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Store the locally opened tabs. |
| [sync](sync.md) | `suspend fun sync(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`, localId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SyncStatus`](../../mozilla.components.concept.sync/-sync-status/index.md)<br>Syncs the remote tabs. |
| [warmUp](warm-up.md) | `open suspend fun warmUp(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Make sure underlying database connections are established. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
