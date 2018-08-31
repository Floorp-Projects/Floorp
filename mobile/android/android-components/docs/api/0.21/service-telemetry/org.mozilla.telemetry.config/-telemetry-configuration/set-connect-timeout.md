---
title: TelemetryConfiguration.setConnectTimeout - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setConnectTimeout](./set-connect-timeout.html)

# setConnectTimeout

`open fun setConnectTimeout(connectTimeout: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Sets a specified timeout value, in milliseconds, to be used when opening a communications link to the telemetry endpoint. If the timeout expires before the connection can be established, the ping upload will be retried at a later time. A timeout of zero is interpreted as an infinite timeout.

