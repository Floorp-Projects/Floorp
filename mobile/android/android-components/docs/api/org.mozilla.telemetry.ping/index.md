[android-components](../index.md) / [org.mozilla.telemetry.ping](./index.md)

## Package org.mozilla.telemetry.ping

### Types

| Name | Summary |
|---|---|
| [TelemetryCorePingBuilder](-telemetry-core-ping-builder/index.md) | `open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.md)<br>This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html |
| [TelemetryEventPingBuilder](-telemetry-event-ping-builder/index.md) | `open class TelemetryEventPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.md)<br>A telemetry ping builder for pings of type "focus-event". |
| [TelemetryMobileEventPingBuilder](-telemetry-mobile-event-ping-builder/index.md) | `open class TelemetryMobileEventPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.md)<br>A telemetry ping builder for events of type "mobile-event". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json |
| [TelemetryMobileMetricsPingBuilder](-telemetry-mobile-metrics-ping-builder/index.md) | `open class TelemetryMobileMetricsPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.md)<br>A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json |
| [TelemetryPing](-telemetry-ping/index.md) | `open class TelemetryPing` |
| [TelemetryPingBuilder](-telemetry-ping-builder/index.md) | `abstract class TelemetryPingBuilder` |
| [TelemetryPocketEventPingBuilder](-telemetry-pocket-event-ping-builder/index.md) | `open class TelemetryPocketEventPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.md)<br>A telemetry ping builder for events of type "fire-tv-events". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/dc458113a7a523e60a9ba50e1174a3b1e0cfdc24/schemas/pocket/fire-tv-events/fire-tv-events.1.schema.json |
