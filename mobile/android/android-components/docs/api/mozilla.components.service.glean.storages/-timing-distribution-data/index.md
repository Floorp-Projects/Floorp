[android-components](../../index.md) / [mozilla.components.service.glean.storages](../index.md) / [TimingDistributionData](./index.md)

# TimingDistributionData

`data class TimingDistributionData` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/storages/TimingDistributionsStorageEngine.kt#L192)

This class represents the structure of a timing distribution according to the pipeline schema. It
is meant to help serialize and deserialize data to the correct format for transport and storage,
as well as performing the calculations to determine the correct bucket for each sample.

The bucket index of a given sample is determined with the following function:

```
    i = ⌊n log₂(𝑥)⌋
```

In other words, there are n buckets for each power of 2 magnitude.

The value of 8 for n was determined experimentally based on existing data to have sufficient
resolution.

Samples greater than 10 minutes in length are truncated to 10 minutes.

### Parameters

`category` - of the metric

`name` - of the metric

`values` - a map containing the minimum bucket value mapped to the accumulated count

`sum` - the accumulated sum of all the samples in the timing distribution

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TimingDistributionData(category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`> = mutableMapOf(), sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0)`<br>This class represents the structure of a timing distribution according to the pipeline schema. It is meant to help serialize and deserialize data to the correct format for transport and storage, as well as performing the calculations to determine the correct bucket for each sample. |

### Properties

| Name | Summary |
|---|---|
| [category](category.md) | `val category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the metric |
| [count](count.md) | `val count: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the metric |
| [sum](sum.md) | `var sum: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>the accumulated sum of all the samples in the timing distribution |
| [values](values.md) | `val values: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`>`<br>a map containing the minimum bucket value mapped to the accumulated count |
