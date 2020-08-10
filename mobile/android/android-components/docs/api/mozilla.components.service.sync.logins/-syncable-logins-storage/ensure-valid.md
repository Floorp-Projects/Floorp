[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [ensureValid](./ensure-valid.md)

# ensureValid

`suspend fun ensureValid(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L244)

Overrides [LoginsStorage.ensureValid](../../mozilla.components.concept.storage/-logins-storage/ensure-valid.md)

### Exceptions

`InvalidRecordException` - On both expected errors (malformed [login](ensure-valid.md#mozilla.components.service.sync.logins.SyncableLoginsStorage$ensureValid(mozilla.components.concept.storage.Login)/login), [login](ensure-valid.md#mozilla.components.service.sync.logins.SyncableLoginsStorage$ensureValid(mozilla.components.concept.storage.Login)/login)
already exists in store, etc. See [InvalidRecordException.reason](#) for details) and
unexpected errors (IO failure, rust panics, etc)