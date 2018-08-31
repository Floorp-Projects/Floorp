---
title: SettingsMeasurement.SettingsProvider - 
---

[org.mozilla.telemetry.measurement](../../index.html) / [SettingsMeasurement](../index.html) / [SettingsProvider](./index.html)

# SettingsProvider

`interface SettingsProvider`

A generic interface for implementations that can provide settings values.

### Functions

| [containsKey](contains-key.html) | `abstract fun containsKey(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if a settings value is available for the given key. |
| [getValue](get-value.html) | `abstract fun getValue(key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)<br>Get the setting value for the given key. |
| [release](release.html) | `abstract fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the provider that we finished reading from it and that it can release resources now. |
| [update](update.html) | `abstract fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify this provider that we are going to read values from it. Some providers might need to perform some actions to be able to provide a fresh set of values. |

### Inheritors

| [SharedPreferenceSettingsProvider](../-shared-preference-settings-provider/index.html) | `open class SharedPreferenceSettingsProvider : `[`SettingsProvider`](./index.md)<br>Setting provider implementation that reads values from SharedPreferences. |

