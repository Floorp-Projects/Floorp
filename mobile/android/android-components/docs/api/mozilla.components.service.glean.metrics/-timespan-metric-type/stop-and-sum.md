[android-components](../../index.md) / [mozilla.components.service.glean.metrics](../index.md) / [TimespanMetricType](index.md) / [stopAndSum](./stop-and-sum.md)

# stopAndSum

`fun stopAndSum(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/metrics/TimespanMetricType.kt#L49)

Stop tracking time for the provided metric. Add the elapsed time to the time currently
stored in the metric. This will record an error if no [start](start.md) was called.

