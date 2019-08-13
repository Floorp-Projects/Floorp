[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [TimingDistributionMetricType](index.md) / [start](./start.md)

# start

`fun start(): `[`GleanTimerId`](../../mozilla.components.service.glean.timing/-glean-timer-id.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/TimingDistributionMetricType.kt#L45)

Start tracking time for the provided metric and [GleanTimerId](../../mozilla.components.service.glean.timing/-glean-timer-id.md). This
records an error if it’s already tracking time (i.e. start was already
called with no corresponding [stopAndAccumulate](stop-and-accumulate.md)): in that case the original
start time will be preserved.

