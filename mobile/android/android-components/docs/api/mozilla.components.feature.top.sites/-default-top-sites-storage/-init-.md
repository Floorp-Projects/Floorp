[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [DefaultTopSitesStorage](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`DefaultTopSitesStorage(pinnedSitesStorage: `[`PinnedSiteStorage`](../-pinned-site-storage/index.md)`, historyStorage: `[`PlacesHistoryStorage`](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md)`, defaultTopSites: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>> = listOf(), coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`

Default implementation of [TopSitesStorage](../-top-sites-storage/index.md).

### Parameters

`pinnedSitesStorage` - An instance of [PinnedSiteStorage](../-pinned-site-storage/index.md), used for storing pinned sites.

`historyStorage` - An instance of [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md), used for retrieving top frecent
sites from history.

`defaultTopSites` - A list containing a title to url pair of default top sites to be added
to the [PinnedSiteStorage](../-pinned-site-storage/index.md).