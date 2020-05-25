[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](index.md) / [submitCrashTelemetry](./submit-crash-telemetry.md)

# submitCrashTelemetry

`fun submitCrashTelemetry(crash: `[`Crash`](../-crash/index.md)`, then: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): Job` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/CrashReporter.kt#L126)

Submit a crash report to all registered telemetry services.

