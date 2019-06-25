[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [MozillaSocorroService](./index.md)

# MozillaSocorroService

`class MozillaSocorroService : `[`CrashReporterService`](../-crash-reporter-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/MozillaSocorroService.kt#L23)

A [CrashReporterService](../-crash-reporter-service/index.md) implementation uploading crash reports to crash-stats.mozilla.com.

### Parameters

`applicationContext` - The application [Context](#).

`appName` - A human-readable app name. This name is used on crash-stats.mozilla.com to filter crashes by app.
    The name needs to be whitelisted for the server to accept the crash.
    [File a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro) if you would like to get your
    app added to the whitelist.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MozillaSocorroService(applicationContext: <ERROR CLASS>, appName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A [CrashReporterService](../-crash-reporter-service/index.md) implementation uploading crash reports to crash-stats.mozilla.com. |

### Functions

| Name | Summary |
|---|---|
| [report](report.md) | `fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).`fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md). |
