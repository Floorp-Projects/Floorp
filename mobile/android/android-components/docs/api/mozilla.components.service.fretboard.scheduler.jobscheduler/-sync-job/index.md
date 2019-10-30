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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
