[android-components](../index.md) / [mozilla.components.lib.crash.service](./index.md)

## Package mozilla.components.lib.crash.service

### Types

| Name | Summary |
|---|---|
| [CrashReporterService](-crash-reporter-service/index.md) | `interface CrashReporterService`<br>Interface to be implemented by external services that accept crash reports. |
| [CrashTelemetryService](-crash-telemetry-service/index.md) | `interface CrashTelemetryService`<br>Interface to be implemented by external services that collect telemetry about crash reports. |
| [GleanCrashReporterService](-glean-crash-reporter-service/index.md) | `class GleanCrashReporterService : `[`CrashTelemetryService`](-crash-telemetry-service/index.md)<br>A [CrashReporterService](-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md), [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) and [Crash.NativeCodeCrash](../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective [mozilla.components.service.glean.private.CounterMetricType](../mozilla.components.service.glean.private/-counter-metric-type.md). |
| [MozillaSocorroService](-mozilla-socorro-service/index.md) | `class MozillaSocorroService : `[`CrashReporterService`](-crash-reporter-service/index.md)<br>A [CrashReporterService](-crash-reporter-service/index.md) implementation uploading crash reports to crash-stats.mozilla.com. |
| [SendCrashReportService](-send-crash-report-service/index.md) | `class SendCrashReportService` |
| [SendCrashTelemetryService](-send-crash-telemetry-service/index.md) | `class SendCrashTelemetryService` |
| [SentryService](-sentry-service/index.md) | `class SentryService : `[`CrashReporterService`](-crash-reporter-service/index.md)<br>A [CrashReporterService](-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server. |
