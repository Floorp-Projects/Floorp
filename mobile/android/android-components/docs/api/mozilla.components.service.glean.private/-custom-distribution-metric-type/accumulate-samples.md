[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [CustomDistributionMetricType](index.md) / [accumulateSamples](./accumulate-samples.md)

# accumulateSamples

`fun accumulateSamples(samples: `[`LongArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/CustomDistributionMetricType.kt#L53)

Overrides [HistogramBase.accumulateSamples](../-histogram-base/accumulate-samples.md)

Accumulates the provided samples in the metric.

The unit of the samples is entirely defined by the user. We encourage the author of the
metric to provide a `unit` parameter in the `metrics.yaml` file, but that has no effect
in the client and there is no unit conversion performed here.

### Parameters

`samples` - the [LongArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html) holding the samples to be recorded by the metric.