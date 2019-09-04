[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](index.md) / [submitReport](./submit-report.md)

# submitReport

`fun submitReport(crash: `[`Crash`](../-crash/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/CrashReporter.kt#L78)

Submit a crash report to all registered services.

Note: This method may block and perform I/O on the calling thread.

