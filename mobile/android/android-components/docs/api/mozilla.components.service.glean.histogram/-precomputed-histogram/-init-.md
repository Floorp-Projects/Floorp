[android-components](../../index.md) / [mozilla.components.service.glean.histogram](../index.md) / [PrecomputedHistogram](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PrecomputedHistogram(rangeMin: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, rangeMax: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, bucketCount: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, histogramType: `[`HistogramType`](../../mozilla.components.service.glean.private/-histogram-type/index.md)`, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0)`

This class represents the structure of a custom distribution. It is meant
to help serialize and deserialize data to the correct format for transport and
storage, as well as including helper functions to calculate the bucket sizes.

### Parameters

`rangeMin` - the minimum value that can be represented

`rangeMax` - the maximum value that can be represented

`bucketCount` - total number of buckets

`histogramType` - the [HistogramType](../../mozilla.components.service.glean.private/-histogram-type/index.md) representing the bucket layout

`values` - a map containing the bucket index mapped to the accumulated count

`sum` - the accumulated sum of all the samples in the custom distribution