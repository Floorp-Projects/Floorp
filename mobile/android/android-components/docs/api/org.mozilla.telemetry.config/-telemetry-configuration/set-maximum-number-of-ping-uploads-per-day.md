[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setMaximumNumberOfPingUploadsPerDay](./set-maximum-number-of-ping-uploads-per-day.md)

# setMaximumNumberOfPingUploadsPerDay

`open fun setMaximumNumberOfPingUploadsPerDay(maximumNumberOfPingUploadsPerDay: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L397)

Set the maximum number of pings uploaded per day. This limit is enforced for every type. If you have 2 ping types and set a limit of 100 pings per day then in total 200 pings per day could be uploaded.

