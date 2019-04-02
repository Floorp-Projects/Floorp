[android-components](../../index.md) / [mozilla.components.service.glean.metrics](../index.md) / [TimespanMetricType](index.md) / [start](./start.md)

# start

`fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/metrics/TimespanMetricType.kt#L37)

Start tracking time for the provided metric. This records an error if it’s
already tracking time (i.e. start was already called with no corresponding
[stopAndSum](stop-and-sum.md)): in that case the original start time will be preserved.

