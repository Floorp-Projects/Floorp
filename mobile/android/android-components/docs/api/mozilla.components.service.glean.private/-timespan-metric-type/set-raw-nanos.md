[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimespanMetricType](index.md) / [setRawNanos](./set-raw-nanos.md)

# setRawNanos

`fun setRawNanos(elapsedNanos: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimespanMetricType.kt#L143)

Explicitly set the timespan value, in nanoseconds.

This API should only be used if your library or application requires recording
times in a way that can not make use of [start](start.md)/[stop](stop.md)/[cancel](cancel.md).

Care should be taken using this if the ping lifetime might contain more than one
timespan measurement.  To be safe, [setRawNanos](./set-raw-nanos.md) should generally be followed by
sending a custom ping containing the timespan.

### Parameters

`elapsedNanos` - The elapsed time to record, in nanoseconds.