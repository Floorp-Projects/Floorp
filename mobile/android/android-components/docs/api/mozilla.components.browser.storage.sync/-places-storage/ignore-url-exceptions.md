[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesStorage](index.md) / [ignoreUrlExceptions](./ignore-url-exceptions.md)

# ignoreUrlExceptions

`protected inline fun ignoreUrlExceptions(operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesStorage.kt#L62)

Runs [block](ignore-url-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$ignoreUrlExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/block) described by [operation](ignore-url-exceptions.md#mozilla.components.browser.storage.sync.PlacesStorage$ignoreUrlExceptions(kotlin.String, kotlin.Function0((kotlin.Unit)))/operation), ignoring and logging any thrown [UrlParseFailed](#) exceptions.

