[android-components](../../index.md) / [mozilla.components.browser.search](../index.md) / [SearchEngineManager](index.md) / [load](./load.md)

# load

`@Synchronized suspend fun load(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SearchEngine`](../-search-engine/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/search/src/main/java/mozilla/components/browser/search/SearchEngineManager.kt#L37)

Asynchronously load search engines from providers. Inherits caller's [CoroutineScope.coroutineContext](#).

