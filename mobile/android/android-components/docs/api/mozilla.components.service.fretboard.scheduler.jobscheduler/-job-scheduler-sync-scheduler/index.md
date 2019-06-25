[android-components](../../index.md) / [mozilla.components.service.fretboard.scheduler.jobscheduler](../index.md) / [JobSchedulerSyncScheduler](./index.md)

# JobSchedulerSyncScheduler

`class JobSchedulerSyncScheduler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/scheduler/jobscheduler/JobSchedulerSyncScheduler.kt#L19)

Class used to schedule sync of experiment
configuration from the server

### Parameters

`context` - context

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JobSchedulerSyncScheduler(context: <ERROR CLASS>)`<br>Class used to schedule sync of experiment configuration from the server |

### Functions

| Name | Summary |
|---|---|
| [schedule](schedule.md) | `fun schedule(jobInfo: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedule sync with the constrains specified`fun schedule(jobId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, serviceName: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Schedule sync with the default constraints (once a day) |
