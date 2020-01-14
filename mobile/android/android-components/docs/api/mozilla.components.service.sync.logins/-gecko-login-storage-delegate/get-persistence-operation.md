[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [GeckoLoginStorageDelegate](index.md) / [getPersistenceOperation](./get-persistence-operation.md)

# getPersistenceOperation

`fun getPersistenceOperation(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`, savedLogin: `[`ServerPassword`](../-server-password.md)`?): `[`Operation`](../-operation/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegate.kt#L114)

Returns whether an existing login record should be [UPDATE](#)d or a new one [CREATE](#)d, based
on the saved [ServerPassword](../-server-password.md) and new [Login](../../mozilla.components.concept.storage/-login/index.md).

