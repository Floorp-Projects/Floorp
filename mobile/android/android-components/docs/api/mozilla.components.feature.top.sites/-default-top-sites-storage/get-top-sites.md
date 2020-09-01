[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [DefaultTopSitesStorage](index.md) / [getTopSites](./get-top-sites.md)

# getTopSites

`suspend fun getTopSites(totalSites: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, includeFrecent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/DefaultTopSitesStorage.kt#L66)

Overrides [TopSitesStorage.getTopSites](../-top-sites-storage/get-top-sites.md)

Return a unified list of top sites based on the given number of sites desired.
If `includeFrecent` is true, fill in any missing top sites with frecent top site results.

### Parameters

`totalSites` - A total number of sites that will be retrieve if possible.

`includeFrecent` - If true, includes frecent top site results.