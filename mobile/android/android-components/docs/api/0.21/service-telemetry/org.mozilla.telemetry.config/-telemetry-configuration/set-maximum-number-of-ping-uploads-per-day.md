---
title: TelemetryConfiguration.setMaximumNumberOfPingUploadsPerDay - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setMaximumNumberOfPingUploadsPerDay](./set-maximum-number-of-ping-uploads-per-day.html)

# setMaximumNumberOfPingUploadsPerDay

`open fun setMaximumNumberOfPingUploadsPerDay(maximumNumberOfPingUploadsPerDay: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Set the maximum number of pings uploaded per day. This limit is enforced for every type. If you have 2 ping types and set a limit of 100 pings per day then in total 200 pings per day could be uploaded.

