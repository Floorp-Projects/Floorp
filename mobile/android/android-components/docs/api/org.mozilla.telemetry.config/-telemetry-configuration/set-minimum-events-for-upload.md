[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setMinimumEventsForUpload](./set-minimum-events-for-upload.md)

# setMinimumEventsForUpload

`open fun setMinimumEventsForUpload(minimumEventsForUpload: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L243)

Set the minimum number of telemetry events that need to be fired before even trying to upload an event ping. The default value is 3. The minimum needs to be &gt;= 1.

