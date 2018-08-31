---
title: SyncJob - 
---

[mozilla.components.service.fretboard.scheduler.jobscheduler](../index.html) / [SyncJob](./index.html)

# SyncJob

`abstract class SyncJob : JobService`

JobScheduler job used to updating the list of experiments

### Constructors

| [&lt;init&gt;](-init-.html) | `SyncJob()`<br>JobScheduler job used to updating the list of experiments |

### Functions

| [getFretboard](get-fretboard.html) | `abstract fun getFretboard(): `[`Fretboard`](../../mozilla.components.service.fretboard/-fretboard/index.html)<br>Used to provide the instance of Fretboard the app is using |
| [onStartJob](on-start-job.html) | `open fun onStartJob(params: JobParameters): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onStopJob](on-stop-job.html) | `open fun onStopJob(params: JobParameters?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

