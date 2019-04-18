[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimingDistributionMetricType](index.md) / [start](./start.md)

# start

`fun start(timerId: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimingDistributionMetricType.kt#L46)

Start tracking time for the provided metric and associated object. This
records an error if it’s already tracking time (i.e. start was already
called with no corresponding [stopAndAccumulate](stop-and-accumulate.md)): in that case the original
start time will be preserved.

### Parameters

`timerId` - The object to associate with this timing.  This allows
for concurrent timing of events associated with different objects to the
same timespan metric.