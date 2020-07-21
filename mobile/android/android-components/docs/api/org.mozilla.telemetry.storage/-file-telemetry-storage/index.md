[android-components](../../index.md) / [org.mozilla.telemetry.storage](../index.md) / [FileTelemetryStorage](./index.md)

# FileTelemetryStorage

`open class FileTelemetryStorage : `[`TelemetryStorage`](../-telemetry-storage/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/storage/FileTelemetryStorage.java#L37)

TelemetryStorage implementation that stores pings as files on disk.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FileTelemetryStorage(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`, serializer: `[`TelemetryPingSerializer`](../../org.mozilla.telemetry.serialize/-telemetry-ping-serializer/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [countStoredPings](count-stored-pings.md) | `open fun countStoredPings(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [process](process.md) | `open fun process(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, callback: `[`TelemetryStorageCallback`](../-telemetry-storage/-telemetry-storage-callback/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [store](store.md) | `open fun store(ping: `[`TelemetryPing`](../../org.mozilla.telemetry.ping/-telemetry-ping/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
