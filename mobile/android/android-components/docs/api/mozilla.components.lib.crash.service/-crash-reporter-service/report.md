[android-components](../../index.md) / [mozilla.components.lib.crash.service](../index.md) / [CrashReporterService](index.md) / [report](./report.md)

# report

`abstract fun report(crash: `[`UncaughtExceptionCrash`](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L37)

Submits a crash report for this [Crash.UncaughtExceptionCrash](../../mozilla.components.lib.crash/-crash/-uncaught-exception-crash/index.md).

**Return**
Unique crash report identifier that can be used by/with this crash reporter service
to find this reported crash - or null if no identifier can be provided.

`abstract fun report(crash: `[`NativeCodeCrash`](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L45)

Submits a crash report for this [Crash.NativeCodeCrash](../../mozilla.components.lib.crash/-crash/-native-code-crash/index.md).

**Return**
Unique crash report identifier that can be used by/with this crash reporter service
to find this reported crash - or null if no identifier can be provided.

`abstract fun report(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../mozilla.components.support.base.crash/-breadcrumb/index.md)`>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/service/CrashReporterService.kt#L53)

Submits a caught exception report for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html).

**Return**
Unique crash report identifier that can be used by/with this crash reporter service
to find this reported crash - or null if no identifier can be provided.

