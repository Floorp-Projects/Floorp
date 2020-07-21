[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [PocketIdMeasurement](./index.md)

# PocketIdMeasurement

`open class PocketIdMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/PocketIdMeasurement.java#L17)

A unique, randomly generated UUID for this pocket client for fire-tv instance. This is distinct from the telemetry clientId. The clientId should not be able to be tied to the pocketId in any way.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PocketIdMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
