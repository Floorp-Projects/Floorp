[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [HistogramBase](index.md) / [accumulateSamples](./accumulate-samples.md)

# accumulateSamples

`abstract fun accumulateSamples(samples: `[`LongArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/HistogramBase.kt#L17)

Accumulates the provided samples in the metric.

### Parameters

`samples` - the [LongArray](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long-array/index.html) holding the samples to be recorded by the metric.