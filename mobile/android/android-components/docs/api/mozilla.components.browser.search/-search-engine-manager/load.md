[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngineManager](index.md) / [load](./load.md)

# load

`@Synchronized suspend fun load(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): Deferred<`[`SearchEngineList`](../../mozilla.components.browser.search.provider/-search-engine-list/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L45)

Asynchronously load search engines from providers. Inherits caller's [CoroutineScope.coroutineContext](#).

