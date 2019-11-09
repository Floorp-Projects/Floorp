[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [GlobalSyncableStoreProvider](index.md) / [configureKeyStorage](./configure-key-storage.md)

# configureKeyStorage

`fun configureKeyStorage(ks: `[`SecureAbove22Preferences`](../../mozilla.components.lib.dataprotect/-secure-above22-preferences/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L96)

Set an instance of [SecureAbove22Preferences](../../mozilla.components.lib.dataprotect/-secure-above22-preferences/index.md) used for accessing an encryption key for [SyncEngine.Passwords](../../mozilla.components.service.fxa/-sync-engine/-passwords.md).

### Parameters

`ks` - An instance of [SecureAbove22Preferences](../../mozilla.components.lib.dataprotect/-secure-above22-preferences/index.md).