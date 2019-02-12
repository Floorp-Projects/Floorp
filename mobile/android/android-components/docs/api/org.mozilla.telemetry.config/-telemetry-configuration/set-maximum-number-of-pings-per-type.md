[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setMaximumNumberOfPingsPerType](./set-maximum-number-of-pings-per-type.md)

# setMaximumNumberOfPingsPerType

`open fun setMaximumNumberOfPingsPerType(maximumNumberOfPingsPerType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L373)

Set the maximum number of pings that will be stored for a given ping type. If more types are in the local store then pings will be removed (oldest first). For this to happen the maximum needs to be reached without any successful upload.

