---
title: JobSchedulerSyncScheduler.schedule - 
---

[mozilla.components.service.fretboard.scheduler.jobscheduler](../index.html) / [JobSchedulerSyncScheduler](index.html) / [schedule](./schedule.html)

# schedule

`fun schedule(jobInfo: JobInfo): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Schedule sync with the constrains specified

### Parameters

`jobInfo` - object with the job constraints`fun schedule(jobId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, serviceName: ComponentName): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Schedule sync with the default constraints
(once a day)

### Parameters

`jobId` - unique identifier of the job

`serviceName` - object with the service to run