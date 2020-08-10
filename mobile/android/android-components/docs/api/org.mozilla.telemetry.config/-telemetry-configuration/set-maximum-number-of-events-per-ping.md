[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setMaximumNumberOfEventsPerPing](./set-maximum-number-of-events-per-ping.md)

# setMaximumNumberOfEventsPerPing

`open fun setMaximumNumberOfEventsPerPing(maximumNumberOfEventsPerPing: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L356)

Set the maximum number of events per ping. If this limit is reached a ping will built and stored automatically. The number of stored and uploaded pings might be limited too.

