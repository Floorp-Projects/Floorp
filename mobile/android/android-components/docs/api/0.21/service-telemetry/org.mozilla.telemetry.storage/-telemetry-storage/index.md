---
title: TelemetryStorage - 
---

[org.mozilla.telemetry.storage](../index.html) / [TelemetryStorage](./index.html)

# TelemetryStorage

`interface TelemetryStorage`

### Types

| [TelemetryStorageCallback](-telemetry-storage-callback/index.html) | `interface TelemetryStorageCallback` |

### Functions

| [countStoredPings](count-stored-pings.html) | `abstract fun countStoredPings(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [process](process.html) | `abstract fun process(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, callback: `[`TelemetryStorageCallback`](-telemetry-storage-callback/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [store](store.html) | `abstract fun store(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| [FileTelemetryStorage](../-file-telemetry-storage/index.html) | `open class FileTelemetryStorage : `[`TelemetryStorage`](./index.md)<br>TelemetryStorage implementation that stores pings as files on disk. |

