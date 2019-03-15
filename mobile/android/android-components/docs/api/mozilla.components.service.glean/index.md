[android-components](../index.md) / [mozilla.components.service.glean](./index.md)

## Package mozilla.components.service.glean

### Types

| Name | Summary |
|---|---|
| [BooleanMetricType](-boolean-metric-type/index.md) | `data class BooleanMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording boolean metrics. |
| [CommonMetricData](-common-metric-data/index.md) | `interface CommonMetricData`<br>This defines the common set of data shared across all the different metric types. |
| [CounterMetricType](-counter-metric-type/index.md) | `data class CounterMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording counter metrics. |
| [DatetimeMetricType](-datetime-metric-type/index.md) | `data class DatetimeMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording datetime metrics. |
| [EventMetricType](-event-metric-type/index.md) | `data class EventMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording events. |
| [Glean](-glean.md) | `object Glean : `[`GleanInternalAPI`](-glean-internal-a-p-i/index.md) |
| [GleanInternalAPI](-glean-internal-a-p-i/index.md) | `open class GleanInternalAPI` |
| [LabeledMetricType](-labeled-metric-type/index.md) | `data class LabeledMetricType<T> : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for labeled metrics. |
| [Lifetime](-lifetime/index.md) | `enum class Lifetime`<br>Enumeration of different metric lifetimes. |
| [StringListMetricType](-string-list-metric-type/index.md) | `data class StringListMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording string list metrics. |
| [StringMetricType](-string-metric-type/index.md) | `data class StringMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording string metrics. |
| [TimeUnit](-time-unit/index.md) | `enum class TimeUnit`<br>Enumeration of different resolutions supported by the Timespan metric type. |
| [TimespanMetricType](-timespan-metric-type/index.md) | `data class TimespanMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording timespans. |
| [UuidMetricType](-uuid-metric-type/index.md) | `data class UuidMetricType : `[`CommonMetricData`](-common-metric-data/index.md)<br>This implements the developer facing API for recording uuids. |
