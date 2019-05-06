[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [StringListMetricType](index.md) / [set](./set.md)

# set

`fun set(value: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/StringListMetricType.kt#L59)

Sets a string list to one or more metric stores. If any string exceeds the maximum string
length or if the list exceeds the maximum length it will be truncated.

### Parameters

`value` - This is a user defined string list.