---
title: SettingsMeasurement.SharedPreferenceSettingsProvider - 
---

[org.mozilla.telemetry.measurement](../../index.html) / [SettingsMeasurement](../index.html) / [SharedPreferenceSettingsProvider](./index.html)

# SharedPreferenceSettingsProvider

`open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](../-settings-provider/index.html)

Setting provider implementation that reads values from SharedPreferences.

### Constructors

| [&lt;init&gt;](-init-.html) | `SharedPreferenceSettingsProvider()`<br>Setting provider implementation that reads values from SharedPreferences. |

### Functions

| [containsKey](contains-key.html) | `open fun containsKey(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getValue](get-value.html) | `open fun getValue(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [release](release.html) | `open fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [update](update.html) | `open fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

