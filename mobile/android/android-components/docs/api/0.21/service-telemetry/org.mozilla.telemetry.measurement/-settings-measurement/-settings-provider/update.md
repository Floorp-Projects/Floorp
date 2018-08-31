---
title: SettingsMeasurement.SettingsProvider.update - 
---

[org.mozilla.telemetry.measurement](../../index.html) / [SettingsMeasurement](../index.html) / [SettingsProvider](index.html) / [update](./update.html)

# update

`abstract fun update(configuration: `[`TelemetryConfiguration`](../../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Notify this provider that we are going to read values from it. Some providers might need to perform some actions to be able to provide a fresh set of values.

