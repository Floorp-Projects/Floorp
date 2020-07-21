[android-components](../../../index.md) / [org.mozilla.telemetry.measurement](../../index.md) / [SettingsMeasurement](../index.md) / [SettingsProvider](./index.md)

# SettingsProvider

`interface SettingsProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SettingsMeasurement.java#L20)

A generic interface for implementations that can provide settings values.

### Functions

| Name | Summary |
|---|---|
| [containsKey](contains-key.md) | `abstract fun containsKey(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if a settings value is available for the given key. |
| [getValue](get-value.md) | `abstract fun getValue(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)<br>Get the setting value for the given key. |
| [release](release.md) | `abstract fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the provider that we finished reading from it and that it can release resources now. |
| [update](update.md) | `abstract fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify this provider that we are going to read values from it. Some providers might need to perform some actions to be able to provide a fresh set of values. |

### Inheritors

| Name | Summary |
|---|---|
| [SharedPreferenceSettingsProvider](../-shared-preference-settings-provider/index.md) | `open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](./index.md)<br>Setting provider implementation that reads values from SharedPreferences. |
