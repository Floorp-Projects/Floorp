[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [SentryService](./index.md)

# SentryService

`class SentryService : `[`CrashReporterService`](../-crash-reporter-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/SentryService.kt#L29)

A [CrashReporterService](../-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server.

This implementation will add default tags to every sent crash report (like the used Android Components version)
prefixed with "ac.".

### Parameters

`context` - The application [Context](#).

`dsn` - Data Source Name of the Sentry server.

`tags` - A list of additional tags that will be sent together with crash reports.

`environment` - An optional, environment name string or null to set none

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SentryService(context: <ERROR CLASS>, dsn: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tags: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), environment: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, sendEventForNativeCrashes: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, clientFactory: SentryClientFactory = AndroidSentryClientFactory(context))`<br>A [CrashReporterService](../-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server. |

### Functions

| Name | Summary |
|---|---|
| [report](report.md) | `fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md). |
