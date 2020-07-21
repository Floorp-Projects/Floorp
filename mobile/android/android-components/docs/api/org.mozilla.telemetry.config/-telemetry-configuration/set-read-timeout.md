[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setReadTimeout](./set-read-timeout.md)

# setReadTimeout

`open fun setReadTimeout(readTimeout: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L211)

Sets the read timeout to a specified timeout, in milliseconds. A non-zero value specifies the timeout when reading from the telemetry endpoint. If the timeout expires before there is data available for read, the ping upload will be retried later. A timeout of zero is interpreted as an infinite timeout.

