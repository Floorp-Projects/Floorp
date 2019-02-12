[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [CrashReporterService](index.md) / [report](./report.md)

# report

`abstract fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L16)

Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).

`abstract fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L21)

Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md).

