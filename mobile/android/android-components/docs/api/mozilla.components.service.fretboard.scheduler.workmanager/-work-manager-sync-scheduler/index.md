[android-components](../../index.md) / [mozilla.components.service.fretboard.scheduler.workmanager](../index.md) / [WorkManagerSyncScheduler](./index.md)

# WorkManagerSyncScheduler

`class WorkManagerSyncScheduler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/scheduler/workmanager/WorkManagerSyncScheduler.kt#L19)

Class used to schedule sync of experiment
configuration from the server using WorkManager

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WorkManagerSyncScheduler(context: <ERROR CLASS>)`<br>Class used to schedule sync of experiment configuration from the server using WorkManager |

### Functions

| Name | Summary |
|---|---|
| [schedule](schedule.md) | `fun schedule(worker: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<out `[`SyncWorker`](../-sync-worker/index.md)`>, interval: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`> = Pair(1, TimeUnit.DAYS)): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedule sync with the default constraints (once a day and charging) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
