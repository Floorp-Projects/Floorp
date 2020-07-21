[android-components](../../index.md) / [org.mozilla.telemetry.ping](../index.md) / [TelemetryMobileMetricsPingBuilder](./index.md)

# TelemetryMobileMetricsPingBuilder

`open class TelemetryMobileMetricsPingBuilder : `[`TelemetryPingBuilder`](../-telemetry-ping-builder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/ping/TelemetryMobileMetricsPingBuilder.java#L28)

A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryMobileMetricsPingBuilder(snapshots: JSONObject, configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [TYPE](-t-y-p-e.md) | `static val TYPE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [build](../-telemetry-ping-builder/build.md) | `open fun build(): `[`TelemetryPing`](../-telemetry-ping/index.md) |
| [canBuild](../-telemetry-ping-builder/can-build.md) | `open fun canBuild(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [generateDocumentId](../-telemetry-ping-builder/generate-document-id.md) | `open fun generateDocumentId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [getConfiguration](../-telemetry-ping-builder/get-configuration.md) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md) |
| [getType](../-telemetry-ping-builder/get-type.md) | `open fun getType(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
