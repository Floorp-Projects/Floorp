---
title: JobSchedulerTelemetryScheduler - 
---

[org.mozilla.telemetry.schedule.jobscheduler](../index.html) / [JobSchedulerTelemetryScheduler](./index.html)

# JobSchedulerTelemetryScheduler

`open class JobSchedulerTelemetryScheduler : `[`TelemetryScheduler`](../../org.mozilla.telemetry.schedule/-telemetry-scheduler/index.html)

TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads.

### Constructors

| [&lt;init&gt;](-init-.html) | `JobSchedulerTelemetryScheduler()`<br>TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads. |

### Properties

| [JOB_ID](-j-o-b_-i-d.html) | `static val JOB_ID: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| [scheduleUpload](schedule-upload.html) | `open fun scheduleUpload(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

