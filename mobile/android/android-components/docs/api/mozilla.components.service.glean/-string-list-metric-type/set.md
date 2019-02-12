[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [StringListMetricType](index.md) / [set](./set.md)

# set

`fun set(value: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/StringListMetricType.kt#L82)

Sets a string list to one or more metric stores.

### Parameters

`value` - This is a user defined string list. The maximum length of each string in the
    list is defined by [MAX_STRING_LENGTH](-m-a-x_-s-t-r-i-n-g_-l-e-n-g-t-h.md), while the maximum length of the list itself is
    defined by [StringListsStorageEngine.MAX_LIST_LENGTH_VALUE](#).  If a longer list is passed
    into this function, then the additional values will be dropped from the list and the list,
    up to the [StringListsStorageEngine.MAX_LIST_LENGTH_VALUE](#), will still be recorded.