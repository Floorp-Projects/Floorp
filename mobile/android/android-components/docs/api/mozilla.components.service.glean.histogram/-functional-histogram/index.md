[android-components](../../index.md) / [mozilla.components.service.glean.histogram](../index.md) / [FunctionalHistogram](./index.md)

# FunctionalHistogram

`data class FunctionalHistogram` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/histogram/FunctionalHistogram.kt#L27)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FunctionalHistogram(logBase: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`, bucketsPerMagnitude: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0)`<br>This class represents a histogram where the bucketing is performed by a function, rather than pre-computed buckets. It is meant to help serialize and deserialize data to the correct format for transport and storage, as well as performing the calculations to determine the correct bucket for each sample. |

### Properties

| Name | Summary |
|---|---|
| [bucketsPerMagnitude](buckets-per-magnitude.md) | `val bucketsPerMagnitude: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html) |
| [count](count.md) | `val count: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [logBase](log-base.md) | `val logBase: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html) |
| [sum](sum.md) | `var sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the accumulated sum of all the samples in the histogram |
| [values](values.md) | `val values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>`<br>a map containing the minimum bucket value mapped to the accumulated count |
