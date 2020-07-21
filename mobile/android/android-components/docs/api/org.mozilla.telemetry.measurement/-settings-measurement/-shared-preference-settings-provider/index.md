[android-components](../../../index.md) / [org.mozilla.telemetry.measurement](../../index.md) / [SettingsMeasurement](../index.md) / [SharedPreferenceSettingsProvider](./index.md)

# SharedPreferenceSettingsProvider

`open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](../-settings-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SettingsMeasurement.java#L46)

Setting provider implementation that reads values from SharedPreferences.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SharedPreferenceSettingsProvider()`<br>Setting provider implementation that reads values from SharedPreferences. |

### Functions

| Name | Summary |
|---|---|
| [containsKey](contains-key.md) | `open fun containsKey(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getValue](get-value.md) | `open fun getValue(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [release](release.md) | `open fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [update](update.md) | `open fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
