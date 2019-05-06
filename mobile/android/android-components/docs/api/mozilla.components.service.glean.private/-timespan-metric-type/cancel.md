[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimespanMetricType](index.md) / [cancel](./cancel.md)

# cancel

`fun cancel(timerId: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimespanMetricType.kt#L79)

Abort a previous [start](start.md) call. No error is recorded if no [start](start.md) was called.

### Parameters

`timerId` - The object to associate with this timing.  This allows
for concurrent timing of events associated with different objects to the
same timespan metric.