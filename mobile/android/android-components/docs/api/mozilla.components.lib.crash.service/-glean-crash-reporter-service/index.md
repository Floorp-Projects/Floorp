[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [GleanCrashReporterService](./index.md)

# GleanCrashReporterService

`class GleanCrashReporterService : `[`CrashReporterService`](../-crash-reporter-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/GleanCrashReporterService.kt#L21)

A [CrashReporterService](../-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this
crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md) and
[Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective
[mozilla.components.service.glean.private.CounterMetricType](../../mozilla.components.service.glean.private/-counter-metric-type/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GleanCrashReporterService(context: <ERROR CLASS>, file: `[`File`](https://developer.android.com/reference/java/io/File.html)` = File(context.applicationInfo.dataDir, CRASH_FILE_NAME))`<br>A [CrashReporterService](../-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md) and [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective [mozilla.components.service.glean.private.CounterMetricType](../../mozilla.components.service.glean.private/-counter-metric-type/index.md). |

### Properties

| Name | Summary |
|---|---|
| [context](context.md) | `val context: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [report](report.md) | `fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md). |

### Companion Object Properties

| Name | Summary |
|---|---|
| [CRASH_FILE_NAME](-c-r-a-s-h_-f-i-l-e_-n-a-m-e.md) | `const val CRASH_FILE_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [NATIVE_CODE_CRASH_KEY](-n-a-t-i-v-e_-c-o-d-e_-c-r-a-s-h_-k-e-y.md) | `const val NATIVE_CODE_CRASH_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [UNCAUGHT_EXCEPTION_KEY](-u-n-c-a-u-g-h-t_-e-x-c-e-p-t-i-o-n_-k-e-y.md) | `const val UNCAUGHT_EXCEPTION_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
