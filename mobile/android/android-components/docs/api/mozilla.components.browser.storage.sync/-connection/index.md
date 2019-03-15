[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [Connection](./index.md)

# Connection

`interface Connection : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/Connection.kt#L27)

A slight abstraction over [PlacesApi](#).

A single reader is assumed here, which isn't a limitation placed on use by [PlacesApi](#).
We can switch to pooling multiple readers as the need arises. Underneath, these are connections
to a SQLite database, and so opening and maintaining them comes with a memory/IO burden.

Writer is always the same, as guaranteed by [PlacesApi](#).

### Functions

| Name | Summary |
|---|---|
| [reader](reader.md) | `abstract fun reader(): ReadablePlacesConnectionInterface` |
| [sync](sync.md) | `abstract fun sync(syncInfo: SyncAuthInfo): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [writer](writer.md) | `abstract fun writer(): WritablePlacesConnectionInterface` |
