[android-components](../../index.md) / [mozilla.components.service.experiments.scheduler.jobscheduler](../index.md) / [JobSchedulerSyncScheduler](./index.md)

# JobSchedulerSyncScheduler

`class JobSchedulerSyncScheduler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/scheduler/jobscheduler/JobSchedulerSyncScheduler.kt#L19)

Class used to schedule sync of experiment
configuration from the server

### Parameters

`context` - context

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JobSchedulerSyncScheduler(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`)`<br>Class used to schedule sync of experiment configuration from the server |

### Functions

| Name | Summary |
|---|---|
| [schedule](schedule.md) | `fun schedule(jobInfo: `[`JobInfo`](https://developer.android.com/reference/android/app/job/JobInfo.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedule sync with the constrains specified`fun schedule(jobId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, serviceName: `[`ComponentName`](https://developer.android.com/reference/android/content/ComponentName.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedule sync with the default constraints (once a day) |
