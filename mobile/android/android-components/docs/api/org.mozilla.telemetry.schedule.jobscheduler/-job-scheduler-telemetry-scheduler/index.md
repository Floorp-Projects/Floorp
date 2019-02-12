[android-components](../../index.md) / [org.mozilla.telemetry.schedule.jobscheduler](../index.md) / [JobSchedulerTelemetryScheduler](./index.md)

# JobSchedulerTelemetryScheduler

`open class JobSchedulerTelemetryScheduler : `[`TelemetryScheduler`](../../org.mozilla.telemetry.schedule/-telemetry-scheduler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/schedule/jobscheduler/JobSchedulerTelemetryScheduler.java#L18)

TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JobSchedulerTelemetryScheduler()`<br>`JobSchedulerTelemetryScheduler(jobId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [scheduleUpload](schedule-upload.md) | `open fun scheduleUpload(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
