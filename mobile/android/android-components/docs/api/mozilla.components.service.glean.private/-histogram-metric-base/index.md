[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [HistogramMetricBase](./index.md)

# HistogramMetricBase

`interface HistogramMetricBase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/HistogramMetricBase.kt#L11)

A common interface to be implemented by all the histogram-like metric types
supported by the Glean SDK.

### Functions

| Name | Summary |
|---|---|
| [accumulateSamples](accumulate-samples.md) | `abstract fun accumulateSamples(samples: `[`LongArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Accumulates the provided samples in the metric. |

### Inheritors

| Name | Summary |
|---|---|
| [CustomDistributionMetricType](../-custom-distribution-metric-type/index.md) | `data class CustomDistributionMetricType : `[`CommonMetricData`](../-common-metric-data/index.md)`, `[`HistogramMetricBase`](./index.md)<br>This implements the developer facing API for recording custom distribution metrics. |
| [MemoryDistributionMetricType](../-memory-distribution-metric-type/index.md) | `data class MemoryDistributionMetricType : `[`CommonMetricData`](../-common-metric-data/index.md)`, `[`HistogramMetricBase`](./index.md)<br>This implements the developer facing API for recording memory distribution metrics. |
| [TimingDistributionMetricType](../-timing-distribution-metric-type/index.md) | `data class TimingDistributionMetricType : `[`CommonMetricData`](../-common-metric-data/index.md)`, `[`HistogramMetricBase`](./index.md)<br>This implements the developer facing API for recording timing distribution metrics. |
