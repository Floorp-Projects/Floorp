[android-components](../../index.md) / [mozilla.components.service.glean.storages](../index.md) / [TimingDistributionData](./index.md)

# TimingDistributionData

`data class TimingDistributionData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/storages/TimingDistributionsStorageEngine.kt#L170)

This class represents the structure of a timing distribution according to the pipeline schema. It
is meant to help serialize and deserialize data to the correct format for transport and storage,
as well as including a helper function to calculate the bucket sizes.

### Parameters

`category` - of the metric

`name` - of the metric

`bucketCount` - total number of buckets

`rangeMin` - the minimum value that can be represented

`rangeMax` - the maximum value that can be represented

`histogramType` - the [HistogramType](../../mozilla.components.service.glean.private/-histogram-type/index.md) representing the bucket layout

`values` - a map containing the bucket index mapped to the accumulated count

`sum` - the accumulated sum of all the samples in the timing distribution

`timeUnit` - the base [TimeUnit](../../mozilla.components.service.glean.private/-time-unit/index.md) of the bucket values

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TimingDistributionData(category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, bucketCount: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = DEFAULT_BUCKET_COUNT, rangeMin: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_RANGE_MIN, rangeMax: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = DEFAULT_RANGE_MAX, histogramType: `[`HistogramType`](../../mozilla.components.service.glean.private/-histogram-type/index.md)` = HistogramType.Exponential, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0, timeUnit: `[`TimeUnit`](../../mozilla.components.service.glean.private/-time-unit/index.md)` = TimeUnit.Millisecond)`<br>This class represents the structure of a timing distribution according to the pipeline schema. It is meant to help serialize and deserialize data to the correct format for transport and storage, as well as including a helper function to calculate the bucket sizes. |

### Properties

| Name | Summary |
|---|---|
| [bucketCount](bucket-count.md) | `val bucketCount: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>total number of buckets |
| [category](category.md) | `val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the metric |
| [count](count.md) | `val count: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [histogramType](histogram-type.md) | `val histogramType: `[`HistogramType`](../../mozilla.components.service.glean.private/-histogram-type/index.md)<br>the [HistogramType](../../mozilla.components.service.glean.private/-histogram-type/index.md) representing the bucket layout |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the metric |
| [rangeMax](range-max.md) | `val rangeMax: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the maximum value that can be represented |
| [rangeMin](range-min.md) | `val rangeMin: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the minimum value that can be represented |
| [sum](sum.md) | `var sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the accumulated sum of all the samples in the timing distribution |
| [timeUnit](time-unit.md) | `val timeUnit: `[`TimeUnit`](../../mozilla.components.service.glean.private/-time-unit/index.md)<br>the base [TimeUnit](../../mozilla.components.service.glean.private/-time-unit/index.md) of the bucket values |
| [values](values.md) | `val values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>`<br>a map containing the bucket index mapped to the accumulated count |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_BUCKET_COUNT](-d-e-f-a-u-l-t_-b-u-c-k-e-t_-c-o-u-n-t.md) | `const val DEFAULT_BUCKET_COUNT: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [DEFAULT_RANGE_MAX](-d-e-f-a-u-l-t_-r-a-n-g-e_-m-a-x.md) | `const val DEFAULT_RANGE_MAX: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [DEFAULT_RANGE_MIN](-d-e-f-a-u-l-t_-r-a-n-g-e_-m-i-n.md) | `const val DEFAULT_RANGE_MIN: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
