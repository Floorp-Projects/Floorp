[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorage](index.md) / [add](./add.md)

# add

`abstract fun add(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L225)

Inserts the provided login into the database, returning it's id.

This function ignores values in metadata fields (`timesUsed`,
`timeCreated`, `timeLastUsed`, and `timePasswordChanged`).

If login has an empty id field, then a GUID will be
generated automatically. The format of generated guids
are left up to the implementation of LoginsStorage (in
practice the [DatabaseLoginsStorage](#) generates 12-character
base64url (RFC 4648) encoded strings, and [MemoryLoginsStorage](#)
generates strings using [java.util.UUID.toString](https://developer.android.com/reference/java/util/UUID.html#toString()))

This will return an error result if a GUID is provided but
collides with an existing record, or if the provided record
is invalid (missing password, hostname, or doesn't have exactly
one of formSubmitURL and httpRealm).

**RejectsWith**
[IdCollisionException](../-id-collision-exception.md) if a nonempty id is provided, and

**RejectsWith**
[InvalidRecordException](../-invalid-record-exception.md) if the record is invalid.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

