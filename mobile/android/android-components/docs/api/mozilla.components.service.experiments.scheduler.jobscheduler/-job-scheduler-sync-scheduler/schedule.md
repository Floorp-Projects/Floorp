[android-components](../../index.md) / [mozilla.components.service.experiments.scheduler.jobscheduler](../index.md) / [JobSchedulerSyncScheduler](index.md) / [schedule](./schedule.md)

# schedule

`fun schedule(jobInfo: `[`JobInfo`](https://developer.android.com/reference/android/app/job/JobInfo.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/scheduler/jobscheduler/JobSchedulerSyncScheduler.kt#L27)

Schedule sync with the constrains specified

### Parameters

`jobInfo` - object with the job constraints`fun schedule(jobId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, serviceName: `[`ComponentName`](https://developer.android.com/reference/android/content/ComponentName.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/scheduler/jobscheduler/JobSchedulerSyncScheduler.kt#L38)

Schedule sync with the default constraints
(once a day)

### Parameters

`jobId` - unique identifier of the job

`serviceName` - object with the service to run