[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [EventsMeasurement](./index.md)

# EventsMeasurement

`open class EventsMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/EventsMeasurement.java#L29)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EventsMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)`<br>`EventsMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`, filename: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [add](add.md) | `open fun add(event: `[`TelemetryEvent`](../../org.mozilla.telemetry.event/-telemetry-event/index.md)`): `[`EventsMeasurement`](./index.md) |
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [getEventCount](get-event-count.md) | `open fun getEventCount(): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
