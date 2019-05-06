[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimespanMetricType](index.md) / [stopAndSum](./stop-and-sum.md)

# stopAndSum

`fun stopAndSum(timerId: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimespanMetricType.kt#L59)

Stop tracking time for the provided metric and associated object. Add the
elapsed time to the time currently stored in the metric. This will record
an error if no [start](start.md) was called.

### Parameters

`timerId` - The object to associate with this timing.  This allows
for concurrent timing of events associated with different objects to the
same timespan metric.