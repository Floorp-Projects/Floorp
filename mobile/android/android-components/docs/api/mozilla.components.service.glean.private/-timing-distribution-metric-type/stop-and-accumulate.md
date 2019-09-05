[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimingDistributionMetricType](index.md) / [stopAndAccumulate](./stop-and-accumulate.md)

# stopAndAccumulate

`fun stopAndAccumulate(timerId: `[`GleanTimerId`](../../mozilla.components.service.glean.timing/-glean-timer-id.md)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimingDistributionMetricType.kt#L62)

Stop tracking time for the provided metric and associated timer id. Add a
count to the corresponding bucket in the timing distribution.
This will record an error if no [start](start.md) was called.

### Parameters

`timerId` - The [GleanTimerId](../../mozilla.components.service.glean.timing/-glean-timer-id.md) to associate with this timing.  This allows
for concurrent timing of events associated with different ids to the
same timespan metric.