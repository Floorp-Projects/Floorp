---
title: TelemetryHolder - 
---

[org.mozilla.telemetry](../index.html) / [TelemetryHolder](./index.html)

# TelemetryHolder

`open class TelemetryHolder`

Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better.

### Constructors

| [&lt;init&gt;](-init-.html) | `TelemetryHolder()`<br>Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better. |

### Functions

| [get](get.html) | `open static fun get(): `[`Telemetry`](../-telemetry/index.html) |
| [set](set.html) | `open static fun set(telemetry: `[`Telemetry`](../-telemetry/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

