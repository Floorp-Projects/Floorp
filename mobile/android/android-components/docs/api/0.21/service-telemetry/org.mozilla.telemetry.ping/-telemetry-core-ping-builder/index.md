---
title: TelemetryCorePingBuilder - 
---

[org.mozilla.telemetry.ping](../index.html) / [TelemetryCorePingBuilder](./index.html)

# TelemetryCorePingBuilder

`open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](../-telemetry-ping-builder/index.html)

This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html

### Constructors

| [&lt;init&gt;](-init-.html) | `TelemetryCorePingBuilder(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`)` |

### Properties

| [TYPE](-t-y-p-e.html) | `static val TYPE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| [getDefaultSearchMeasurement](get-default-search-measurement.html) | `open fun getDefaultSearchMeasurement(): `[`DefaultSearchMeasurement`](../../org.mozilla.telemetry.measurement/-default-search-measurement/index.html) |
| [getExperimentsMeasurement](get-experiments-measurement.html) | `open fun getExperimentsMeasurement(): `[`ExperimentsMeasurement`](../../org.mozilla.telemetry.measurement/-experiments-measurement/index.html) |
| [getSearchesMeasurement](get-searches-measurement.html) | `open fun getSearchesMeasurement(): `[`SearchesMeasurement`](../../org.mozilla.telemetry.measurement/-searches-measurement/index.html) |
| [getSessionCountMeasurement](get-session-count-measurement.html) | `open fun getSessionCountMeasurement(): `[`SessionCountMeasurement`](../../org.mozilla.telemetry.measurement/-session-count-measurement/index.html) |
| [getSessionDurationMeasurement](get-session-duration-measurement.html) | `open fun getSessionDurationMeasurement(): `[`SessionDurationMeasurement`](../../org.mozilla.telemetry.measurement/-session-duration-measurement/index.html) |

### Inherited Functions

| [build](../-telemetry-ping-builder/build.html) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.html) |
| [canBuild](../-telemetry-ping-builder/can-build.html) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [generateDocumentId](../-telemetry-ping-builder/generate-document-id.html) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](../-telemetry-ping-builder/get-configuration.html) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html) |
| [getType](../-telemetry-ping-builder/get-type.html) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

