[android-components](../../index.md) / [org.mozilla.telemetry.schedule.jobscheduler](../index.md) / [TelemetryJobService](./index.md)

# TelemetryJobService

`open class TelemetryJobService : `[`JobService`](https://developer.android.com/reference/android/app/job/JobService.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/schedule/jobscheduler/TelemetryJobService.java#L26)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryJobService()` |

### Functions

| Name | Summary |
|---|---|
| [onStartJob](on-start-job.md) | `open fun onStartJob(params: `[`JobParameters`](https://developer.android.com/reference/android/app/job/JobParameters.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopJob](on-stop-job.md) | `open fun onStopJob(params: `[`JobParameters`](https://developer.android.com/reference/android/app/job/JobParameters.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [uploadPingsInBackground](upload-pings-in-background.md) | `open fun uploadPingsInBackground(task: `[`AsyncTask`](https://developer.android.com/reference/android/os/AsyncTask.html)`<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>, parameters: `[`JobParameters`](https://developer.android.com/reference/android/app/job/JobParameters.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
