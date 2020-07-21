[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [RemoteTabsStorage](index.md) / [store](./store.md)

# store

`suspend fun store(tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/RemoteTabsStorage.kt#L39)

Store the locally opened tabs.

### Parameters

`tabs` - The list of opened tabs, for all opened non-private windows, on this device.