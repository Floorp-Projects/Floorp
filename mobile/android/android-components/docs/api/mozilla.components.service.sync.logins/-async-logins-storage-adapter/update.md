[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [update](./update.md)

# update

`open fun update(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L351)

Overrides [AsyncLoginsStorage.update](../-async-logins-storage/update.md)

Updates the fields in the provided record.

This will return an error if `login.id` does not refer to
a record that exists in the database, or if the provided record
is invalid (missing password, hostname, or doesn't have exactly
one of formSubmitURL and httpRealm).

Like `add`, this function will ignore values in metadata
fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and
`timePasswordChanged`).

**RejectsWith**
[NoSuchRecordException](../-no-such-record-exception.md) if the login does not exist.

**RejectsWith**
[InvalidRecordException](../-invalid-record-exception.md) if the update would create an invalid record.

**RejectsWith**
[LoginsStorageException](../-logins-storage-exception.md) if the storage is locked, and on unexpected
    errors (IO failure, rust panics, etc)

