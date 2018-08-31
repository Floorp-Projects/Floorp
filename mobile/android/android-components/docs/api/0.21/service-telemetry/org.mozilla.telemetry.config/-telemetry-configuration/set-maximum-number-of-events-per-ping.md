---
title: TelemetryConfiguration.setMaximumNumberOfEventsPerPing - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setMaximumNumberOfEventsPerPing](./set-maximum-number-of-events-per-ping.html)

# setMaximumNumberOfEventsPerPing

`open fun setMaximumNumberOfEventsPerPing(maximumNumberOfEventsPerPing: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Set the maximum number of events per ping. If this limit is reached a ping will built and stored automatically. The number of stored and uploaded pings might be limited too.

