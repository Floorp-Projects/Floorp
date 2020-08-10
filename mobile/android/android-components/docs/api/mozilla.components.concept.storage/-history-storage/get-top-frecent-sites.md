[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [getTopFrecentSites](./get-top-frecent-sites.md)

# getTopFrecentSites

`abstract suspend fun getTopFrecentSites(numItems: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopFrecentSiteInfo`](../-top-frecent-site-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L78)

Returns a list of the top frecent site infos limited by the given number of items
sorted by most to least frecent.

### Parameters

`numItems` - the number of top frecent sites to return in the list.

**Return**
a list of the [TopFrecentSiteInfo](../-top-frecent-site-info/index.md), most frecent first.

