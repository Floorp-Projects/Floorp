[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [CommonMetricData](./index.md)

# CommonMetricData

`interface CommonMetricData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/CommonMetricData.kt#L32)

This defines the common set of data shared across all the different
metric types.

### Properties

| Name | Summary |
|---|---|
| [category](category.md) | `abstract val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [disabled](disabled.md) | `abstract val disabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [identifier](identifier.md) | `open val identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [lifetime](lifetime.md) | `abstract val lifetime: `[`Lifetime`](../-lifetime/index.md) |
| [name](name.md) | `abstract val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [sendInPings](send-in-pings.md) | `abstract val sendInPings: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |

### Functions

| Name | Summary |
|---|---|
| [shouldRecord](should-record.md) | `open fun shouldRecord(logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [BooleanMetricType](../-boolean-metric-type/index.md) | `data class BooleanMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording boolean metrics. |
| [CounterMetricType](../-counter-metric-type/index.md) | `data class CounterMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording counter metrics. |
| [CustomDistributionMetricType](../-custom-distribution-metric-type/index.md) | `data class CustomDistributionMetricType : `[`CommonMetricData`](./index.md)`, `[`HistogramMetricBase`](../-histogram-metric-base/index.md)<br>This implements the developer facing API for recording custom distribution metrics. |
| [DatetimeMetricType](../-datetime-metric-type/index.md) | `data class DatetimeMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording datetime metrics. |
| [EventMetricType](../-event-metric-type/index.md) | `data class EventMetricType<ExtraKeysEnum : `[`Enum`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-enum/index.html)`<`[`ExtraKeysEnum`](../-event-metric-type/index.md#ExtraKeysEnum)`>> : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording events. |
| [LabeledMetricType](../-labeled-metric-type/index.md) | `data class LabeledMetricType<T> : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for labeled metrics. |
| [MemoryDistributionMetricType](../-memory-distribution-metric-type/index.md) | `data class MemoryDistributionMetricType : `[`CommonMetricData`](./index.md)`, `[`HistogramMetricBase`](../-histogram-metric-base/index.md)<br>This implements the developer facing API for recording memory distribution metrics. |
| [StringListMetricType](../-string-list-metric-type/index.md) | `data class StringListMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording string list metrics. |
| [StringMetricType](../-string-metric-type/index.md) | `data class StringMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording string metrics. |
| [TimespanMetricType](../-timespan-metric-type/index.md) | `data class TimespanMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording timespans. |
| [TimingDistributionMetricType](../-timing-distribution-metric-type/index.md) | `data class TimingDistributionMetricType : `[`CommonMetricData`](./index.md)`, `[`HistogramMetricBase`](../-histogram-metric-base/index.md)<br>This implements the developer facing API for recording timing distribution metrics. |
| [UuidMetricType](../-uuid-metric-type/index.md) | `data class UuidMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording uuids. |
