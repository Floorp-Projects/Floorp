[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [WorkManagerSyncDispatcher](./index.md)

# WorkManagerSyncDispatcher

`class WorkManagerSyncDispatcher : `[`SyncDispatcher`](../-sync-dispatcher/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sync/src/main/java/mozilla/components/feature/sync/WorkManagerSyncDispatcher.kt#L77)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WorkManagerSyncDispatcher(stores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, syncScope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, account: `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [isSyncActive](is-sync-active.md) | `fun isSyncActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [startPeriodicSync](start-periodic-sync.md) | `fun startPeriodicSync(unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`, period: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Periodic background syncing is mainly intended to reduce workload when we sync during application startup. |
| [stopPeriodicSync](stop-periodic-sync.md) | `fun stopPeriodicSync(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [syncNow](sync-now.md) | `fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [workersStateChanged](workers-state-changed.md) | `fun workersStateChanged(isRunning: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
