[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [getTopFrecentSites](./get-top-frecent-sites.md)

# getTopFrecentSites

`open suspend fun getTopFrecentSites(numItems: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopFrecentSiteInfo`](../../mozilla.components.concept.storage/-top-frecent-site-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L103)

Overrides [HistoryStorage.getTopFrecentSites](../../mozilla.components.concept.storage/-history-storage/get-top-frecent-sites.md)

Returns a list of the top frecent site infos limited by the given number of items
sorted by most to least frecent.

### Parameters

`numItems` - the number of top frecent sites to return in the list.

**Return**
a list of the [TopFrecentSiteInfo](../../mozilla.components.concept.storage/-top-frecent-site-info/index.md), most frecent first.

