---
title: TelemetryScheduler - 
---

[org.mozilla.telemetry.schedule](../index.html) / [TelemetryScheduler](./index.html)

# TelemetryScheduler

`interface TelemetryScheduler`

### Functions

| [scheduleUpload](schedule-upload.html) | `abstract fun scheduleUpload(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| [JobSchedulerTelemetryScheduler](../../org.mozilla.telemetry.schedule.jobscheduler/-job-scheduler-telemetry-scheduler/index.html) | `open class JobSchedulerTelemetryScheduler : `[`TelemetryScheduler`](./index.md)<br>TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads. |

