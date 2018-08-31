---
title: TelemetryPingBuilder - 
---

[org.mozilla.telemetry.ping](../index.html) / [TelemetryPingBuilder](./index.html)

# TelemetryPingBuilder

`abstract class TelemetryPingBuilder`

### Constructors

| [&lt;init&gt;](-init-.html) | `TelemetryPingBuilder(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`, type: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)` |

### Functions

| [build](build.html) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.html) |
| [canBuild](can-build.html) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [generateDocumentId](generate-document-id.html) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](get-configuration.html) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html) |
| [getType](get-type.html) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| [TelemetryCorePingBuilder](../-telemetry-core-ping-builder/index.html) | `open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html |
| [TelemetryEventPingBuilder](../-telemetry-event-ping-builder/index.html) | `open class TelemetryEventPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for pings of type "focus-event". |
| [TelemetryMobileEventPingBuilder](../-telemetry-mobile-event-ping-builder/index.html) | `open class TelemetryMobileEventPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for events of type "mobile-event". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json |
| [TelemetryMobileMetricsPingBuilder](../-telemetry-mobile-metrics-ping-builder/index.html) | `open class TelemetryMobileMetricsPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json |

