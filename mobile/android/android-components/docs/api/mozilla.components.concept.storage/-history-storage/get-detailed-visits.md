[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [getDetailedVisits](./get-detailed-visits.md)

# getDetailedVisits

`abstract suspend fun getDetailedVisits(start: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, end: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = Long.MAX_VALUE, excludeTypes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitType`](../-visit-type/index.md)`> = listOf()): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitInfo`](../-visit-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L47)

Retrieves detailed information about all visits that occurred in the given time range.

### Parameters

`start` - The (inclusive) start time to bound the query.

`end` - The (inclusive) end time to bound the query.

`excludeTypes` - List of visit types to exclude.

**Return**
A list of all visits within the specified range, described by [VisitInfo](../-visit-info/index.md).

