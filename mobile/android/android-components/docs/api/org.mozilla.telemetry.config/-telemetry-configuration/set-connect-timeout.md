[android-components](../../index.md) / [org.mozilla.telemetry.config](../index.md) / [TelemetryConfiguration](index.md) / [setConnectTimeout](./set-connect-timeout.md)

# setConnectTimeout

`open fun setConnectTimeout(connectTimeout: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/config/TelemetryConfiguration.java#L200)

Sets a specified timeout value, in milliseconds, to be used when opening a communications link to the telemetry endpoint. If the timeout expires before the connection can be established, the ping upload will be retried at a later time. A timeout of zero is interpreted as an infinite timeout.

