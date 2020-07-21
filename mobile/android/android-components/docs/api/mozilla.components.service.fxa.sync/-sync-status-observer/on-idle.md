[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncStatusObserver](index.md) / [onIdle](./on-idle.md)

# onIdle

`abstract fun onIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L62)

Gets called at the end of a sync, after every configured syncable has been synchronized.
A set of enabled [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md)s could have changed, so observers are expected to query
[SyncEnginesStorage.getStatus](../../mozilla.components.service.fxa.manager/-sync-engines-storage/get-status.md).

