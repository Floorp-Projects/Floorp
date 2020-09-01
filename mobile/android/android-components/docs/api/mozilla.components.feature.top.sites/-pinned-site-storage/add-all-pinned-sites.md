[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [PinnedSiteStorage](index.md) / [addAllPinnedSites](./add-all-pinned-sites.md)

# addAllPinnedSites

`suspend fun addAllPinnedSites(topSites: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/PinnedSiteStorage.kt#L30)

Adds the given list pinned sites.

### Parameters

`topSites` - A list containing a title to url pair of top sites to be added.

`isDefault` - Whether or not the pinned site added should be a default pinned site. This
is used to identify pinned sites that are added by the application.