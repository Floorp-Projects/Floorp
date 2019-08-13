[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimingDistributionMetricType](index.md) / [accumulateSamples](./accumulate-samples.md)

# accumulateSamples

`fun accumulateSamples(samples: `[`LongArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimingDistributionMetricType.kt#L105)

Overrides [HistogramBase.accumulateSamples](../-histogram-base/accumulate-samples.md)

Accumulates the provided samples in the metric.

Please note that this assumes that the provided samples are already in the
"unit" declared by the instance of the implementing metric type (e.g. if the
implementing class is a [TimingDistributionMetricType](index.md) and the instance this
method was called on is using [TimeUnit.Second](../-time-unit/-second.md), then `samples` are assumed
to be in that unit).

### Parameters

`samples` - the [LongArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html) holding the samples to be recorded by the metric.