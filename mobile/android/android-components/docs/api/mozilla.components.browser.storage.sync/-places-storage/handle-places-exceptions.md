[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesStorage](index.md) / [handlePlacesExceptions](./handle-places-exceptions.md)

# handlePlacesExceptions

`protected inline fun handlePlacesExceptions(operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesStorage.kt#L78)

Runs [block](handle-places-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$handlePlacesExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/block) described by [operation](handle-places-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$handlePlacesExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/operation), ignoring non-fatal exceptions.

