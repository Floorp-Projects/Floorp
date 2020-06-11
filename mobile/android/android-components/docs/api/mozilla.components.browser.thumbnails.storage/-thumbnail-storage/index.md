[android-components](../../index.md) / [mozilla.components.browser.thumbnails.storage](../index.md) / [ThumbnailStorage](./index.md)

# ThumbnailStorage

`class ThumbnailStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/thumbnails/src/main/java/mozilla/components/browser/thumbnails/storage/ThumbnailStorage.kt#L34)

Thumbnail storage layer which handles saving and loading the thumbnail from the disk cache.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ThumbnailStorage(context: <ERROR CLASS>, jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS)
        .asCoroutineDispatcher())`<br>Thumbnail storage layer which handles saving and loading the thumbnail from the disk cache. |

### Functions

| Name | Summary |
|---|---|
| [loadThumbnail](load-thumbnail.md) | `fun loadThumbnail(sessionIdOrUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<<ERROR CLASS>?>`<br>Asynchronously loads a thumbnail [Bitmap](#) for the given session ID or url. |
| [saveThumbnail](save-thumbnail.md) | `fun saveThumbnail(sessionIdOrUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, bitmap: <ERROR CLASS>): Job`<br>Stores the given thumbnail [Bitmap](#) into the disk cache with the provided session ID or url as its key. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
