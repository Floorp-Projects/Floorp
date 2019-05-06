[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [DatetimeMetricType](index.md) / [set](./set.md)

# set

`fun set(value: `[`Date`](https://developer.android.com/reference/java/util/Date.html)` = Date()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/DatetimeMetricType.kt#L37)

Set a datetime value, truncating it to the metric's resolution.

### Parameters

`value` - The [Date](https://developer.android.com/reference/java/util/Date.html) value to set. If not provided, will record the current time.