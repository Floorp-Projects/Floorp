[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [CrashReporterService](./index.md)

# CrashReporterService

`interface CrashReporterService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L12)

Interface to be implemented by external services that accept crash reports.

### Functions

| Name | Summary |
|---|---|
| [report](report.md) | `abstract fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`abstract fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md). |

### Inheritors

| Name | Summary |
|---|---|
| [MozillaSocorroService](../-mozilla-socorro-service/index.md) | `class MozillaSocorroService : `[`CrashReporterService`](./index.md)<br>A [CrashReporterService](./index.md) implementation uploading crash reports to crash-stats.mozilla.com. |
| [SentryService](../-sentry-service/index.md) | `class SentryService : `[`CrashReporterService`](./index.md)<br>A [CrashReporterService](./index.md) implementation that uploads crash reports to a Sentry server. |
