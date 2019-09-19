[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [MemoryDistributionMetricType](index.md) / [accumulateSamples](./accumulate-samples.md)

# accumulateSamples

`fun accumulateSamples(samples: `[`LongArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/MemoryDistributionMetricType.kt#L62)

Overrides [HistogramMetricBase.accumulateSamples](../-histogram-metric-base/accumulate-samples.md)

Accumulates the provided samples, in the unit specified by `memoryUnit`,
to the distribution.

This function is intended for GeckoView use only.

### Parameters

`samples` - the [LongArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html) holding the samples to be recorded by the metric.