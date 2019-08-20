[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [WorkManagerSyncDispatcher](index.md) / [stopPeriodicSync](./stop-periodic-sync.md)

# stopPeriodicSync

`fun stopPeriodicSync(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/WorkManagerSyncManager.kt#L181)

Overrides [SyncDispatcher.stopPeriodicSync](../-sync-dispatcher/stop-periodic-sync.md)

Disables periodic syncing in the background. Currently running syncs may continue until completion.
Safe to call this even if periodic syncing isn't currently enabled.

