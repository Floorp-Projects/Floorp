[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [DefaultTopSitesStorage](./index.md)

# DefaultTopSitesStorage

`class DefaultTopSitesStorage : `[`TopSitesStorage`](../-top-sites-storage/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../-top-sites-storage/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/DefaultTopSitesStorage.kt#L27)

Default implementation of [TopSitesStorage](../-top-sites-storage/index.md).

### Parameters

`pinnedSitesStorage` - An instance of [PinnedSiteStorage](../-pinned-site-storage/index.md), used for storing pinned sites.

`historyStorage` - An instance of [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md), used for retrieving top frecent
sites from history.

`defaultTopSites` - A list containing a title to url pair of default top sites to be added
to the [PinnedSiteStorage](../-pinned-site-storage/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultTopSitesStorage(pinnedSitesStorage: `[`PinnedSiteStorage`](../-pinned-site-storage/index.md)`, historyStorage: `[`PlacesHistoryStorage`](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md)`, defaultTopSites: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>> = listOf(), coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>Default implementation of [TopSitesStorage](../-top-sites-storage/index.md). |

### Properties

| Name | Summary |
|---|---|
| [cachedTopSites](cached-top-sites.md) | `var cachedTopSites: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [addTopSite](add-top-site.md) | `fun addTopSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new top site. |
| [getTopSites](get-top-sites.md) | `suspend fun getTopSites(totalSites: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, includeFrecent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>`<br>Return a unified list of top sites based on the given number of sites desired. If `includeFrecent` is true, fill in any missing top sites with frecent top site results. |
| [removeTopSite](remove-top-site.md) | `fun removeTopSite(topSite: `[`TopSite`](../-top-site/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [TopSite](../-top-site/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
