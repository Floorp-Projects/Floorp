[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimespanMetricType](index.md) / [sumRawNanos](./sum-raw-nanos.md)

# sumRawNanos

`fun sumRawNanos(elapsedNanos: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimespanMetricType.kt#L116)

Add an explicit timespan, in nanoseconds, to the already recorded
timespans.

This API should only be used if your library or application requires recording
times in a way that can not make use of [start](start.md)/[stop](stop.md)/[cancel](cancel.md).

### Parameters

`elapsedNanos` - The elapsed time to record, in nanoseconds.