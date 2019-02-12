[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [StringListMetricType](index.md) / [add](./add.md)

# add

`fun add(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/StringListMetricType.kt#L49)

Appends a string value to one or more string list metric stores.  If the string exceeds the
maximum string length, defined as [MAX_STRING_LENGTH](-m-a-x_-s-t-r-i-n-g_-l-e-n-g-t-h.md), it will be truncated.

If adding the string to the lists would exceed the maximum value defined as
[StringListsStorageEngine.MAX_LIST_LENGTH_VALUE](#), then the storage engine will drop the new
value and it will not be added to the list.

### Parameters

`value` - This is a user defined string value. The maximum length of
    this string is [MAX_STRING_LENGTH](-m-a-x_-s-t-r-i-n-g_-l-e-n-g-t-h.md).