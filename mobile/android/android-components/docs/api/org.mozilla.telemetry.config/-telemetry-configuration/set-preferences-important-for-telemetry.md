[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setPreferencesImportantForTelemetry](./set-preferences-important-for-telemetry.md)

# setPreferencesImportantForTelemetry

`open fun setPreferencesImportantForTelemetry(vararg preferences: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L146)

Set a list of preference keys that are important for telemetry. Some measurements and pings might use this to determine what preferences should be reported.

