[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](./index.md)

# SyncableLoginsStorage

`class SyncableLoginsStorage : `[`LoginsStorage`](../../mozilla.components.concept.storage/-logins-storage/index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`, `[`AutoCloseable`](http://docs.oracle.com/javase/7/docs/api/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L115)

An implementation of [LoginsStorage](../../mozilla.components.concept.storage/-logins-storage/index.md) backed by application-services' `logins` library.
Synchronization support is provided both directly (via [sync](sync.md)) when only syncing this storage layer,
or via [getHandle](get-handle.md) when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncableLoginsStorage(context: <ERROR CLASS>, key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>An implementation of [LoginsStorage](../../mozilla.components.concept.storage/-logins-storage/index.md) backed by application-services' `logins` library. Synchronization support is provided both directly (via [sync](sync.md)) when only syncing this storage layer, or via [getHandle](get-handle.md) when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager](#). |

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `suspend fun add(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [delete](delete.md) | `suspend fun delete(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [ensureValid](ensure-valid.md) | `suspend fun ensureValid(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): <ERROR CLASS>` |
| [get](get.md) | `suspend fun get(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`?` |
| [getByBaseDomain](get-by-base-domain.md) | `suspend fun getByBaseDomain(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` |
| [getHandle](get-handle.md) | `fun getHandle(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>This should be removed. See: https://github.com/mozilla/application-services/issues/1877 |
| [getPotentialDupesIgnoringUsername](get-potential-dupes-ignoring-username.md) | `suspend fun getPotentialDupesIgnoringUsername(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` |
| [importLoginsAsync](import-logins-async.md) | `suspend fun importLoginsAsync(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>): <ERROR CLASS>` |
| [list](list.md) | `suspend fun list(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>` |
| [sync](sync.md) | `suspend fun sync(syncInfo: `[`SyncUnlockInfo`](../-sync-unlock-info.md)`): SyncTelemetryPing`<br>Synchronizes the logins storage layer with a remote layer. If synchronizing multiple stores, avoid using this - prefer setting up sync via FxaAccountManager instead. |
| [touch](touch.md) | `suspend fun touch(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` |
| [update](update.md) | `suspend fun update(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): <ERROR CLASS>` |
| [warmUp](warm-up.md) | `suspend fun warmUp(): <ERROR CLASS>`<br>"Warms up" this storage layer by establishing the database connection. |
| [wipe](wipe.md) | `suspend fun wipe(): <ERROR CLASS>` |
| [wipeLocal](wipe-local.md) | `suspend fun wipeLocal(): <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
