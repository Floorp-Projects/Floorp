[android-components](../../index.md) / [mozilla.components.service.glean.metrics](../index.md) / [TimespanMetricType](index.md) / [testHasValue](./test-has-value.md)

# testHasValue

`fun testHasValue(pingName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getStorageNames().first()): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/metrics/TimespanMetricType.kt#L77)

Tests whether a value is stored for the metric for testing purposes only

### Parameters

`pingName` - represents the name of the ping to retrieve the metric for.  Defaults
    to the either the first value in [defaultStorageDestinations](default-storage-destinations.md) or the first
    value in [sendInPings](send-in-pings.md)

**Return**
true if metric value exists, otherwise false

