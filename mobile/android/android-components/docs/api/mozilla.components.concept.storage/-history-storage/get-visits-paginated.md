[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [getVisitsPaginated](./get-visits-paginated.md)

# getVisitsPaginated

`abstract suspend fun getVisitsPaginated(offset: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, count: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, excludeTypes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitType`](../-visit-type/index.md)`> = listOf()): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitInfo`](../-visit-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L65)

Return a "page" of history results. Each page will have visits in descending order
with respect to their visit timestamps. In the case of ties, their row id will
be used.

Note that you may get surprising results if the items in the database change
while you are paging through records.

### Parameters

`offset` - The offset where the page begins.

`count` - The number of items to return in the page.

`excludeTypes` - List of visit types to exclude.