[android-components](../../index.md) / [org.mozilla.telemetry.schedule](../index.md) / [TelemetryScheduler](./index.md)

# TelemetryScheduler

`interface TelemetryScheduler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/schedule/TelemetryScheduler.java#L9)

### Functions

| Name | Summary |
|---|---|
| [scheduleUpload](schedule-upload.md) | `abstract fun scheduleUpload(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [JobSchedulerTelemetryScheduler](../../org.mozilla.telemetry.schedule.jobscheduler/-job-scheduler-telemetry-scheduler/index.md) | `open class JobSchedulerTelemetryScheduler : `[`TelemetryScheduler`](./index.md)<br>TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads. |
