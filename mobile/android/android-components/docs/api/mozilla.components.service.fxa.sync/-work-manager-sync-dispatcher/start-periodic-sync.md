[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [WorkManagerSyncDispatcher](index.md) / [startPeriodicSync](./start-periodic-sync.md)

# startPeriodicSync

`fun startPeriodicSync(unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`, period: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/WorkManagerSyncManager.kt#L166)

Overrides [SyncDispatcher.startPeriodicSync](../-sync-dispatcher/start-periodic-sync.md)

Periodic background syncing is mainly intended to reduce workload when we sync during
application startup.

