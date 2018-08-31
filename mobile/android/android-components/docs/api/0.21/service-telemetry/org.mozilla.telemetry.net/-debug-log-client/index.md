---
title: DebugLogClient - 
---

[org.mozilla.telemetry.net](../index.html) / [DebugLogClient](./index.html)

# DebugLogClient

`class DebugLogClient : `[`TelemetryClient`](../-telemetry-client/index.html)

This client just prints pings to logcat instead of uploading them. Therefore this client is only
useful for debugging purposes.

### Constructors

| [&lt;init&gt;](-init-.html) | `DebugLogClient(tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>This client just prints pings to logcat instead of uploading them. Therefore this client is only useful for debugging purposes. |

### Functions

| [uploadPing](upload-ping.html) | `fun uploadPing(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`, path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serializedPing: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

