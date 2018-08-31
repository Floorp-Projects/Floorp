---
title: org.mozilla.telemetry.ping - 
---

[org.mozilla.telemetry.ping](./index.html)

## Package org.mozilla.telemetry.ping

### Types

| [TelemetryCorePingBuilder](-telemetry-core-ping-builder/index.html) | `open class TelemetryCorePingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.html)<br>This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html |
| [TelemetryEventPingBuilder](-telemetry-event-ping-builder/index.html) | `open class TelemetryEventPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.html)<br>A telemetry ping builder for pings of type "focus-event". |
| [TelemetryMobileEventPingBuilder](-telemetry-mobile-event-ping-builder/index.html) | `open class TelemetryMobileEventPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.html)<br>A telemetry ping builder for events of type "mobile-event". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json |
| [TelemetryMobileMetricsPingBuilder](-telemetry-mobile-metrics-ping-builder/index.html) | `open class TelemetryMobileMetricsPingBuilder : `[`TelemetryPingBuilder`](-telemetry-ping-builder/index.html)<br>A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json |
| [TelemetryPing](-telemetry-ping/index.html) | `open class TelemetryPing` |
| [TelemetryPingBuilder](-telemetry-ping-builder/index.html) | `abstract class TelemetryPingBuilder` |

