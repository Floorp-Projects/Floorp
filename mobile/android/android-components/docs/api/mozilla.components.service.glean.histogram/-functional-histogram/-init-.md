[android-components](../../index.md) / [mozilla.components.service.glean.histogram](../index.md) / [FunctionalHistogram](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FunctionalHistogram(logBase: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`, bucketsPerMagnitude: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0)`

This class represents a histogram where the bucketing is performed by a
function, rather than pre-computed buckets. It is meant to help serialize
and deserialize data to the correct format for transport and storage, as well
as performing the calculations to determine the correct bucket for each sample.

The bucket index of a given sample is determined with the following function:

```
    i = ⌊n log₂(𝑥)⌋
```

In other words, there are n buckets for each power of 2 magnitude.

### Parameters

`values` - a map containing the minimum bucket value mapped to the accumulated count

`sum` - the accumulated sum of all the samples in the histogram