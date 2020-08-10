[android-components](../../index.md) / [org.mozilla.telemetry.ping](../index.md) / [TelemetryPingBuilder](./index.md)

# TelemetryPingBuilder

`abstract class TelemetryPingBuilder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/ping/TelemetryPingBuilder.java#L22)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryPingBuilder(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`, type: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.md) |
| [canBuild](can-build.md) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [generateDocumentId](generate-document-id.md) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](get-configuration.md) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md) |
| [getType](get-type.md) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [TelemetryCorePingBuilder](../-telemetry-core-ping-builder/index.md) | `open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html |
| [TelemetryEventPingBuilder](../-telemetry-event-ping-builder/index.md) | `open class TelemetryEventPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for pings of type "focus-event". |
| [TelemetryMobileEventPingBuilder](../-telemetry-mobile-event-ping-builder/index.md) | `open class TelemetryMobileEventPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for events of type "mobile-event". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json |
| [TelemetryMobileMetricsPingBuilder](../-telemetry-mobile-metrics-ping-builder/index.md) | `open class TelemetryMobileMetricsPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json |
| [TelemetryPocketEventPingBuilder](../-telemetry-pocket-event-ping-builder/index.md) | `open class TelemetryPocketEventPingBuilder : `[`TelemetryPingBuilder`](./index.md)<br>A telemetry ping builder for events of type "fire-tv-events". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/dc458113a7a523e60a9ba50e1174a3b1e0cfdc24/schemas/pocket/fire-tv-events/fire-tv-events.1.schema.json |
