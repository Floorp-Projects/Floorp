[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [StringMetricType](index.md) / [set](./set.md)

# set

`fun set(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/StringMetricType.kt#L44)

Set a string value.

### Parameters

`value` - This is a user defined string value. If the length of the string
exceeds [MAX_LENGTH_VALUE](#) characters, it will be truncated.