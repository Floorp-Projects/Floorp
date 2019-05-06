[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [StringMetricType](index.md) / [set](./set.md)

# set

`fun set(value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/StringMetricType.kt#L37)

Set a string value.

### Parameters

`value` - This is a user defined string value. If the length of the string exceeds
    the maximum length, it will be truncated.