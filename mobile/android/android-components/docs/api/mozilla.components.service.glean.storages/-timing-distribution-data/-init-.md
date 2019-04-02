[android-components](../../index.md) / [mozilla.components.service.glean.storages](../index.md) / [TimingDistributionData](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TimingDistributionData(category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, bucketCount: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_BUCKET_COUNT, range: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = listOf(DEFAULT_RANGE_MIN, DEFAULT_RANGE_MAX), histogramType: `[`HistogramType`](../../mozilla.components.service.glean.metrics/-histogram-type/index.md)` = HistogramType.Exponential, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0, timeUnit: `[`TimeUnit`](../../mozilla.components.service.glean.metrics/-time-unit/index.md)` = TimeUnit.Millisecond)`

This class represents the structure of a timing distribution according to the pipeline schema. It
is meant to help serialize and deserialize data to the correct format for transport and storage,
as well as including a helper function to calculate the bucket sizes.

### Parameters

`category` - of the metric

`name` - of the metric

`bucketCount` - total number of buckets

`range` - an array always containing 2 elements: the minimum and maximum bucket values

`histogramType` - the [HistogramType](../../mozilla.components.service.glean.metrics/-histogram-type/index.md) representing the bucket layout

`values` - a map containing the bucket index mapped to the accumulated count

`sum` - the accumulated sum of all the samples in the timing distribution

`timeUnit` - the base [TimeUnit](../../mozilla.components.service.glean.metrics/-time-unit/index.md) of the bucket values