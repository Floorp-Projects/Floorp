---
title: SettingsMeasurement - 
---

[org.mozilla.telemetry.measurement](../index.html) / [SettingsMeasurement](./index.html)

# SettingsMeasurement

`open class SettingsMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.html)

### Types

| [SettingsProvider](-settings-provider/index.html) | `interface SettingsProvider`<br>A generic interface for implementations that can provide settings values. |
| [SharedPreferenceSettingsProvider](-shared-preference-settings-provider/index.html) | `open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](-settings-provider/index.html)<br>Setting provider implementation that reads values from SharedPreferences. |

### Constructors

| [&lt;init&gt;](-init-.html) | `SettingsMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`)` |

### Functions

| [flush](flush.html) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Inherited Functions

| [getFieldName](../-telemetry-measurement/get-field-name.html) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

