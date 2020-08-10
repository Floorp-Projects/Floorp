[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [WorkManagerSyncDispatcher](./index.md)

# WorkManagerSyncDispatcher

`class WorkManagerSyncDispatcher : SyncDispatcher, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../-sync-status-observer/index.md)`>, `[`Closeable`](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/WorkManagerSyncManager.kt#L131)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WorkManagerSyncDispatcher(context: <ERROR CLASS>, supportedEngines: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`>)` |

### Functions

| Name | Summary |
|---|---|
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [isSyncActive](is-sync-active.md) | `fun isSyncActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [startPeriodicSync](start-periodic-sync.md) | `fun startPeriodicSync(unit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`, period: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Periodic background syncing is mainly intended to reduce workload when we sync during application startup. |
| [stopPeriodicSync](stop-periodic-sync.md) | `fun stopPeriodicSync(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables periodic syncing in the background. Currently running syncs may continue until completion. Safe to call this even if periodic syncing isn't currently enabled. |
| [syncNow](sync-now.md) | `fun syncNow(reason: `[`SyncReason`](../-sync-reason/index.md)`, debounce: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [workersStateChanged](workers-state-changed.md) | `fun workersStateChanged(isRunning: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
