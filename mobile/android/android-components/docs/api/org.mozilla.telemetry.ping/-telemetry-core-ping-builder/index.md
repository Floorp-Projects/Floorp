[android-components](../../index.md) / [org.mozilla.telemetry.ping](../index.md) / [TelemetryCorePingBuilder](./index.md)

# TelemetryCorePingBuilder

`open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](../-telemetry-ping-builder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/ping/TelemetryCorePingBuilder.java#L31)

This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryCorePingBuilder(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [TYPE](-t-y-p-e.md) | `static val TYPE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [getDefaultSearchMeasurement](get-default-search-measurement.md) | `open fun getDefaultSearchMeasurement(): `[`DefaultSearchMeasurement`](../../org.mozilla.telemetry.measurement/-default-search-measurement/index.md) |
| [getExperimentsMeasurement](get-experiments-measurement.md) | `open fun getExperimentsMeasurement(): `[`ExperimentsMeasurement`](../../org.mozilla.telemetry.measurement/-experiments-measurement/index.md) |
| [getSearchesMeasurement](get-searches-measurement.md) | `open fun getSearchesMeasurement(): `[`SearchesMeasurement`](../../org.mozilla.telemetry.measurement/-searches-measurement/index.md) |
| [getSessionCountMeasurement](get-session-count-measurement.md) | `open fun getSessionCountMeasurement(): `[`SessionCountMeasurement`](../../org.mozilla.telemetry.measurement/-session-count-measurement/index.md) |
| [getSessionDurationMeasurement](get-session-duration-measurement.md) | `open fun getSessionDurationMeasurement(): `[`SessionDurationMeasurement`](../../org.mozilla.telemetry.measurement/-session-duration-measurement/index.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [build](../-telemetry-ping-builder/build.md) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.md) |
| [canBuild](../-telemetry-ping-builder/can-build.md) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [generateDocumentId](../-telemetry-ping-builder/generate-document-id.md) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](../-telemetry-ping-builder/get-configuration.md) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md) |
| [getType](../-telemetry-ping-builder/get-type.md) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
