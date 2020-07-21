[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [CrashReporterService](./index.md)

# CrashReporterService

`interface CrashReporterService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L15)

Interface to be implemented by external services that accept crash reports.

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID to identify this crash reporter service. |
| [name](name.md) | `abstract val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A human-readable name for this crash reporter service (to be displayed in UI). |

### Functions

| Name | Summary |
|---|---|
| [createCrashReportUrl](create-crash-report-url.md) | `abstract fun createCrashReportUrl(identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns a URL to a website with the crash report if possible. Otherwise returns null. |
| [report](report.md) | `abstract fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`abstract fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md).`abstract fun report(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../mozilla.components.support.base.crash/-breadcrumb/index.md)`>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a caught exception report for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MozillaSocorroService](../-mozilla-socorro-service/index.md) | `class MozillaSocorroService : `[`CrashReporterService`](./index.md)<br>A [CrashReporterService](./index.md) implementation uploading crash reports to crash-stats.mozilla.com. |
| [SentryService](../-sentry-service/index.md) | `class SentryService : `[`CrashReporterService`](./index.md)<br>A [CrashReporterService](./index.md) implementation that uploads crash reports to a Sentry server. |
