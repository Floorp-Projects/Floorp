---
title: TelemetryConfiguration.setMaximumNumberOfPingsPerType - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setMaximumNumberOfPingsPerType](./set-maximum-number-of-pings-per-type.html)

# setMaximumNumberOfPingsPerType

`open fun setMaximumNumberOfPingsPerType(maximumNumberOfPingsPerType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Set the maximum number of pings that will be stored for a given ping type. If more types are in the local store then pings will be removed (oldest first). For this to happen the maximum needs to be reached without any successful upload.

