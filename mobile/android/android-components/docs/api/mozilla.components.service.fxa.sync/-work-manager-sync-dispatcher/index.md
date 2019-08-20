[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [WorkManagerSyncDispatcher](./index.md)

# WorkManagerSyncDispatcher

`class WorkManagerSyncDispatcher : `[`SyncDispatcher`](../-sync-dispatcher/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/WorkManagerSyncManager.kt#L110)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WorkManagerSyncDispatcher(stores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)` |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [isSyncActive](is-sync-active.md) | `fun isSyncActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [startPeriodicSync](start-periodic-sync.md) | `fun startPeriodicSync(unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`, period: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Periodic background syncing is mainly intended to reduce workload when we sync during application startup. |
| [stopPeriodicSync](stop-periodic-sync.md) | `fun stopPeriodicSync(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables periodic syncing in the background. Currently running syncs may continue until completion. Safe to call this even if periodic syncing isn't currently enabled. |
| [syncNow](sync-now.md) | `fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, debounce: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [workersStateChanged](workers-state-changed.md) | `fun workersStateChanged(isRunning: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
