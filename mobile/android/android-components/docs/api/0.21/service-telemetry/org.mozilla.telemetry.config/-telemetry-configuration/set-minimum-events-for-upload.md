---
title: TelemetryConfiguration.setMinimumEventsForUpload - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setMinimumEventsForUpload](./set-minimum-events-for-upload.html)

# setMinimumEventsForUpload

`open fun setMinimumEventsForUpload(minimumEventsForUpload: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Set the minimum number of telemetry events that need to be fired before even trying to upload an event ping. The default value is 3. The minimum needs to be &gt;= 1.

