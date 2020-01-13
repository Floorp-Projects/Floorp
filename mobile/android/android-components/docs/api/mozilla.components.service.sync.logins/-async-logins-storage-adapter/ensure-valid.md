[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [ensureValid](./ensure-valid.md)

# ensureValid

`open fun ensureValid(login: `[`ServerPassword`](../-server-password.md)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L417)

Overrides [AsyncLoginsStorage.ensureValid](../-async-logins-storage/ensure-valid.md)

Checks if login already exists and is valid.

**RejectsWith**
[InvalidRecordException](#) On both expected errors (malformed [login](../-async-logins-storage/ensure-valid.md#mozilla.components.service.sync.logins.AsyncLoginsStorage$ensureValid(mozilla.appservices.logins.ServerPassword)/login), [login](../-async-logins-storage/ensure-valid.md#mozilla.components.service.sync.logins.AsyncLoginsStorage$ensureValid(mozilla.appservices.logins.ServerPassword)/login)
already exists in store, etc. See [InvalidRecordException.reason](#) for details) and
unexpected errors (IO failure, rust panics, etc)

