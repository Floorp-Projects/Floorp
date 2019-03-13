[android-components](../index.md) / [mozilla.components.browser.storage.sync](./index.md)

## Package mozilla.components.browser.storage.sync

### Types

| Name | Summary |
|---|---|
| [Connection](-connection/index.md) | `interface Connection : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)<br>An interface which describes a [Closeable](https://developer.android.com/reference/java/io/Closeable.html) connection that provides access to a [PlacesAPI](#). |
| [PlacesHistoryStorage](-places-history-storage/index.md) | `open class PlacesHistoryStorage : `[`HistoryStorage`](../mozilla.components.concept.storage/-history-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>Implementation of the [HistoryStorage](../mozilla.components.concept.storage/-history-storage/index.md) which is backed by a Rust Places lib via [PlacesConnection](#). |

### Type Aliases

| Name | Summary |
|---|---|
| [SyncAuthInfo](-sync-auth-info.md) | `typealias SyncAuthInfo = SyncAuthInfo` |

### Properties

| Name | Summary |
|---|---|
| [AUTOCOMPLETE_SOURCE_NAME](-a-u-t-o-c-o-m-p-l-e-t-e_-s-o-u-r-c-e_-n-a-m-e.md) | `const val AUTOCOMPLETE_SOURCE_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DB_NAME](-d-b_-n-a-m-e.md) | `const val DB_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
