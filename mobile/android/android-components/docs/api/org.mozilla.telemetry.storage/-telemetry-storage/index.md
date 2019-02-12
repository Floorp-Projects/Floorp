[android-components](../../index.md) / [org.mozilla.telemetry.storage](../index.md) / [TelemetryStorage](./index.md)

# TelemetryStorage

`interface TelemetryStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/storage/TelemetryStorage.java#L9)

### Types

| Name | Summary |
|---|---|
| [TelemetryStorageCallback](-telemetry-storage-callback/index.md) | `interface TelemetryStorageCallback` |

### Functions

| Name | Summary |
|---|---|
| [countStoredPings](count-stored-pings.md) | `abstract fun countStoredPings(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [process](process.md) | `abstract fun process(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, callback: `[`TelemetryStorageCallback`](-telemetry-storage-callback/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [store](store.md) | `abstract fun store(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [FileTelemetryStorage](../-file-telemetry-storage/index.md) | `open class FileTelemetryStorage : `[`TelemetryStorage`](./index.md)<br>TelemetryStorage implementation that stores pings as files on disk. |
