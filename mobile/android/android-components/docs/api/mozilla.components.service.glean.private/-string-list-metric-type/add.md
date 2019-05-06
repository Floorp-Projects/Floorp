[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [StringListMetricType](index.md) / [add](./add.md)

# add

`fun add(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/StringListMetricType.kt#L38)

Appends a string value to one or more string list metric stores.  If the string exceeds the
maximum string length or if the list exceeds the maximum length it will be truncated.

### Parameters

`value` - This is a user defined string value. The maximum length of
    this string is [MAX_STRING_LENGTH](#).