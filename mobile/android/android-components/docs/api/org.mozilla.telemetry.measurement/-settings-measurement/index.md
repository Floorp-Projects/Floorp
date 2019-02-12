[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [SettingsMeasurement](./index.md)

# SettingsMeasurement

`open class SettingsMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SettingsMeasurement.java#L16)

### Types

| Name | Summary |
|---|---|
| [SettingsProvider](-settings-provider/index.md) | `interface SettingsProvider`<br>A generic interface for implementations that can provide settings values. |
| [SharedPreferenceSettingsProvider](-shared-preference-settings-provider/index.md) | `open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](-settings-provider/index.md)<br>Setting provider implementation that reads values from SharedPreferences. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SettingsMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
