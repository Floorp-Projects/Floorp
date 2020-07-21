[android-components](../../../index.md) / [org.mozilla.telemetry.measurement](../../index.md) / [SettingsMeasurement](../index.md) / [SettingsProvider](index.md) / [update](./update.md)

# update

`abstract fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SettingsMeasurement.java#L25)

Notify this provider that we are going to read values from it. Some providers might need to perform some actions to be able to provide a fresh set of values.

