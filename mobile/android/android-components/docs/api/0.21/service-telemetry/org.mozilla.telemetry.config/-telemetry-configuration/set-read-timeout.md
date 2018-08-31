---
title: TelemetryConfiguration.setReadTimeout - 
---

[org.mozilla.telemetry.config](../index.html) / [TelemetryConfiguration](index.html) / [setReadTimeout](./set-read-timeout.html)

# setReadTimeout

`open fun setReadTimeout(readTimeout: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TelemetryConfiguration`](index.html)

Sets the read timeout to a specified timeout, in milliseconds. A non-zero value specifies the timeout when reading from the telemetry endpoint. If the timeout expires before there is data available for read, the ping upload will be retried later. A timeout of zero is interpreted as an infinite timeout.

