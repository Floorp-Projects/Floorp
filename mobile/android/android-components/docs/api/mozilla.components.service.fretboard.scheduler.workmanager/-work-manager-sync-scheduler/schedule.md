[android-components](../../index.md) / [mozilla.components.service.fretboard.scheduler.workmanager](../index.md) / [WorkManagerSyncScheduler](index.md) / [schedule](./schedule.md)

# schedule

`fun schedule(worker: `[`Class`](https://developer.android.com/reference/java/lang/Class.html)`<out `[`SyncWorker`](../-sync-worker/index.md)`>, interval: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`> = Pair(1, TimeUnit.DAYS)): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/scheduler/workmanager/WorkManagerSyncScheduler.kt#L25)

Schedule sync with the default constraints
(once a day and charging)

### Parameters

`worker` - worker class