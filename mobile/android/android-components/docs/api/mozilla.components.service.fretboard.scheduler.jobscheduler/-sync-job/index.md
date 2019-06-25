[android-components](../../index.md) / [mozilla.components.service.fretboard.scheduler.jobscheduler](../index.md) / [SyncJob](./index.md)

# SyncJob

`abstract class SyncJob` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/scheduler/jobscheduler/SyncJob.kt#L15)

JobScheduler job used to updating the list of experiments

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncJob()`<br>JobScheduler job used to updating the list of experiments |

### Functions

| Name | Summary |
|---|---|
| [getFretboard](get-fretboard.md) | `abstract fun getFretboard(): `[`Fretboard`](../../mozilla.components.service.fretboard/-fretboard/index.md)<br>Used to provide the instance of Fretboard the app is using |
| [onStartJob](on-start-job.md) | `open fun onStartJob(params: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopJob](on-stop-job.md) | `open fun onStopJob(params: <ERROR CLASS>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
