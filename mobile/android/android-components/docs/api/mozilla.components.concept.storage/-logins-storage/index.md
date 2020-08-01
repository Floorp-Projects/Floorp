[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](./index.md)

# LoginsStorage

`interface LoginsStorage : `[`AutoCloseable`](http://docs.oracle.com/javase/7/docs/api/java/lang/AutoCloseable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L14)

An interface describing a storage layer for logins/passwords.

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `abstract suspend fun add(login: `[`Login`](../-login/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Inserts the provided login into the database, returning it's id. |
| [delete](delete.md) | `abstract suspend fun delete(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Deletes the password with the given ID. |
| [ensureValid](ensure-valid.md) | `abstract suspend fun ensureValid(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Checks if login already exists and is valid. Implementations expected to throw for invalid [login](ensure-valid.md#mozilla.components.concept.storage.LoginsStorage$ensureValid(mozilla.components.concept.storage.Login)/login). |
| [get](get.md) | `abstract suspend fun get(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Login`](../-login/index.md)`?`<br>Fetches a password from the underlying storage layer by its unique identifier. |
| [getByBaseDomain](get-by-base-domain.md) | `abstract suspend fun getByBaseDomain(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>`<br>Fetch the list of logins for some origin from the underlying storage layer. |
| [getPotentialDupesIgnoringUsername](get-potential-dupes-ignoring-username.md) | `abstract suspend fun getPotentialDupesIgnoringUsername(login: `[`Login`](../-login/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>`<br>Fetch the list of potential duplicate logins from the underlying storage layer, ignoring username. |
| [importLoginsAsync](import-logins-async.md) | `abstract suspend fun importLoginsAsync(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>): <ERROR CLASS>`<br>Bulk-import of a list of [Login](../-login/index.md). Storage must be empty; implementations expected to throw otherwise. |
| [list](list.md) | `abstract suspend fun list(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>`<br>Fetches the full list of logins from the underlying storage layer. |
| [touch](touch.md) | `abstract suspend fun touch(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Marks the login with the given [guid](touch.md#mozilla.components.concept.storage.LoginsStorage$touch(kotlin.String)/guid) as `in-use`. |
| [update](update.md) | `abstract suspend fun update(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the fields in the provided record. |
| [wipe](wipe.md) | `abstract suspend fun wipe(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes all login records. These deletions will be synced to the server on the next call to sync. |
| [wipeLocal](wipe-local.md) | `abstract suspend fun wipeLocal(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears out all local state, bringing us back to the state before the first write (or sync). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [SyncableLoginsStorage](../../mozilla.components.service.sync.logins/-syncable-logins-storage/index.md) | `class SyncableLoginsStorage : `[`LoginsStorage`](./index.md)`, `[`SyncableStore`](../../mozilla.components.concept.sync/-syncable-store/index.md)`, `[`AutoCloseable`](http://docs.oracle.com/javase/7/docs/api/java/lang/AutoCloseable.html)<br>An implementation of [LoginsStorage](./index.md) backed by application-services' `logins` library. Synchronization support is provided both directly (via [sync](../../mozilla.components.service.sync.logins/-syncable-logins-storage/sync.md)) when only syncing this storage layer, or via [getHandle](../../mozilla.components.service.sync.logins/-syncable-logins-storage/get-handle.md) when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager](#). |
