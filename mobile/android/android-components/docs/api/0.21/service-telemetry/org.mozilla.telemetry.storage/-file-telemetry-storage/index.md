---
title: FileTelemetryStorage - 
---

[org.mozilla.telemetry.storage](../index.html) / [FileTelemetryStorage](./index.html)

# FileTelemetryStorage

`open class FileTelemetryStorage : `[`TelemetryStorage`](../-telemetry-storage/index.html)

TelemetryStorage implementation that stores pings as files on disk.

### Constructors

| [&lt;init&gt;](-init-.html) | `FileTelemetryStorage(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`, serializer: `[`TelemetryPingSerializer`](../../org.mozilla.telemetry.serialize/-telemetry-ping-serializer/index.html)`)` |

### Functions

| [countStoredPings](count-stored-pings.html) | `open fun countStoredPings(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [process](process.html) | `open fun process(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, callback: `[`TelemetryStorageCallback`](../-telemetry-storage/-telemetry-storage-callback/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [store](store.html) | `open fun store(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

