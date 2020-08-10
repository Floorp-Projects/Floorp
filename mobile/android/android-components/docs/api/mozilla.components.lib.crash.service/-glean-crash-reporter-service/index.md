[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [GleanCrashReporterService](./index.md)

# GleanCrashReporterService

`class GleanCrashReporterService : `[`CrashTelemetryService`](../-crash-telemetry-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/GleanCrashReporterService.kt#L22)

A [CrashReporterService](../-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this
crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md),
[Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) and [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective
[mozilla.components.service.glean.private.CounterMetricType](../../mozilla.components.service.glean.private/-counter-metric-type.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GleanCrashReporterService(context: <ERROR CLASS>, file: `[`File`](http://docs.oracle.com/javase/7/docs/api/java/io/File.html)` = File(context.applicationInfo.dataDir, CRASH_FILE_NAME))`<br>A [CrashReporterService](../-crash-reporter-service/index.md) implementation for recording metrics with Glean.  The purpose of this crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md), [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) and [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md) events and record to the respective [mozilla.components.service.glean.private.CounterMetricType](../../mozilla.components.service.glean.private/-counter-metric-type.md). |

### Properties

| Name | Summary |
|---|---|
| [context](context.md) | `val context: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [record](record.md) | `fun record(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records telemetry for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`fun record(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records telemetry for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md).`fun record(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Records telemetry for this caught [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) (non-crash). |

### Companion Object Properties

| Name | Summary |
|---|---|
| [CAUGHT_EXCEPTION_KEY](-c-a-u-g-h-t_-e-x-c-e-p-t-i-o-n_-k-e-y.md) | `const val CAUGHT_EXCEPTION_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [CRASH_FILE_NAME](-c-r-a-s-h_-f-i-l-e_-n-a-m-e.md) | `const val CRASH_FILE_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FATAL_NATIVE_CODE_CRASH_KEY](-f-a-t-a-l_-n-a-t-i-v-e_-c-o-d-e_-c-r-a-s-h_-k-e-y.md) | `const val FATAL_NATIVE_CODE_CRASH_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [NONFATAL_NATIVE_CODE_CRASH_KEY](-n-o-n-f-a-t-a-l_-n-a-t-i-v-e_-c-o-d-e_-c-r-a-s-h_-k-e-y.md) | `const val NONFATAL_NATIVE_CODE_CRASH_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [UNCAUGHT_EXCEPTION_KEY](-u-n-c-a-u-g-h-t_-e-x-c-e-p-t-i-o-n_-k-e-y.md) | `const val UNCAUGHT_EXCEPTION_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
