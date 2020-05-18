[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [SentryService](./index.md)

# SentryService

`class SentryService : `[`CrashReporterService`](../-crash-reporter-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/SentryService.kt#L35)

A [CrashReporterService](../-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server.

This implementation will add default tags to every sent crash report (like the used Android Components version)
prefixed with "ac.".

### Parameters

`context` - The application [Context](#).

`dsn` - Data Source Name of the Sentry server.

`tags` - A list of additional tags that will be sent together with crash reports.

`environment` - An optional, environment name string or null to set none

`sentryProjectUrl` - Base URL of the Sentry web interface pointing to the app/project.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SentryService(context: <ERROR CLASS>, dsn: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tags: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyMap(), environment: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, sendEventForNativeCrashes: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, sentryProjectUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, clientFactory: SentryClientFactory? = null)`<br>A [CrashReporterService](../-crash-reporter-service/index.md) implementation that uploads crash reports to a Sentry server. |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID to identify this crash reporter service. |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A human-readable name for this crash reporter service (to be displayed in UI). |

### Functions

| Name | Summary |
|---|---|
| [createCrashReportUrl](create-crash-report-url.md) | `fun createCrashReportUrl(identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Returns a URL to a website with the crash report if possible. Otherwise returns null. |
| [report](report.md) | `fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md).`fun report(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../mozilla.components.support.base.crash/-breadcrumb/index.md)`>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Submits a caught exception report for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
