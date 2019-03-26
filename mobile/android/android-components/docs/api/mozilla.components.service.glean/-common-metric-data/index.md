[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [CommonMetricData](./index.md)

# CommonMetricData

`interface CommonMetricData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/CommonMetricData.kt#L31)

This defines the common set of data shared across all the different
metric types.

### Properties

| Name | Summary |
|---|---|
| [category](category.md) | `abstract val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [defaultStorageDestinations](default-storage-destinations.md) | `abstract val defaultStorageDestinations: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Defines the names of the storages the metric defaults to when "default" is used as the destination storage. Note that every metric type will need to override this. |
| [disabled](disabled.md) | `abstract val disabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [identifier](identifier.md) | `open val identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [lifetime](lifetime.md) | `abstract val lifetime: `[`Lifetime`](../-lifetime/index.md) |
| [name](name.md) | `abstract val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [sendInPings](send-in-pings.md) | `abstract val sendInPings: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |

### Functions

| Name | Summary |
|---|---|
| [getStorageNames](get-storage-names.md) | `open fun getStorageNames(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Get the list of storage names the metric will record to. This automatically expands [DEFAULT_STORAGE_NAME](#) to the list of default storages for the metric. |
| [shouldRecord](should-record.md) | `open fun shouldRecord(logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [BooleanMetricType](../-boolean-metric-type/index.md) | `data class BooleanMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording boolean metrics. |
| [CounterMetricType](../-counter-metric-type/index.md) | `data class CounterMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording counter metrics. |
| [DatetimeMetricType](../-datetime-metric-type/index.md) | `data class DatetimeMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording datetime metrics. |
| [EventMetricType](../-event-metric-type/index.md) | `data class EventMetricType<ExtraKeysEnum : `[`Enum`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-enum/index.html)`<`[`ExtraKeysEnum`](../-event-metric-type/index.md#ExtraKeysEnum)`>> : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording events. |
| [LabeledMetricType](../-labeled-metric-type/index.md) | `data class LabeledMetricType<T> : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for labeled metrics. |
| [StringListMetricType](../-string-list-metric-type/index.md) | `data class StringListMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording string list metrics. |
| [StringMetricType](../-string-metric-type/index.md) | `data class StringMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording string metrics. |
| [TimespanMetricType](../-timespan-metric-type/index.md) | `data class TimespanMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording timespans. |
| [TimingDistributionMetricType](../-timing-distribution-metric-type/index.md) | `data class TimingDistributionMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording timing distribution metrics. |
| [UuidMetricType](../-uuid-metric-type/index.md) | `data class UuidMetricType : `[`CommonMetricData`](./index.md)<br>This implements the developer facing API for recording uuids. |
