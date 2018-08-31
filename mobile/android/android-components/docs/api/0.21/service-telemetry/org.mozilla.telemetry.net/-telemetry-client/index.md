---
title: TelemetryClient - 
---

[org.mozilla.telemetry.net](../index.html) / [TelemetryClient](./index.html)

# TelemetryClient

`interface TelemetryClient`

### Functions

| [uploadPing](upload-ping.html) | `abstract fun uploadPing(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`, path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serializedPing: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Inheritors

| [DebugLogClient](../-debug-log-client/index.html) | `class DebugLogClient : `[`TelemetryClient`](./index.md)<br>This client just prints pings to logcat instead of uploading them. Therefore this client is only useful for debugging purposes. |
| [HttpURLConnectionTelemetryClient](../-http-u-r-l-connection-telemetry-client/index.html) | `open class HttpURLConnectionTelemetryClient : `[`TelemetryClient`](./index.md) |

