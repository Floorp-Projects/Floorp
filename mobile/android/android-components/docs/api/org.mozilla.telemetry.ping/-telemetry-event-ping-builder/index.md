[android-components](../../index.md) / [org.mozilla.telemetry.ping](../index.md) / [TelemetryEventPingBuilder](./index.md)

# TelemetryEventPingBuilder

`open class TelemetryEventPingBuilder : `[`TelemetryPingBuilder`](../-telemetry-ping-builder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/ping/TelemetryEventPingBuilder.java#L18)

A telemetry ping builder for pings of type "focus-event".

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryEventPingBuilder(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [TYPE](-t-y-p-e.md) | `static val TYPE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [canBuild](can-build.md) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getEventsMeasurement](get-events-measurement.md) | `open fun getEventsMeasurement(): `[`EventsMeasurement`](../../org.mozilla.telemetry.measurement/-events-measurement/index.md) |
| [getExperimentsMapMeasurement](get-experiments-map-measurement.md) | `open fun getExperimentsMapMeasurement(): `[`ExperimentsMapMeasurement`](../../org.mozilla.telemetry.measurement/-experiments-map-measurement/index.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [build](../-telemetry-ping-builder/build.md) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.md) |
| [generateDocumentId](../-telemetry-ping-builder/generate-document-id.md) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](../-telemetry-ping-builder/get-configuration.md) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md) |
| [getType](../-telemetry-ping-builder/get-type.md) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
