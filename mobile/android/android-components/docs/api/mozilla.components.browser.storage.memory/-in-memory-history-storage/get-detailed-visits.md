[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [getDetailedVisits](./get-detailed-visits.md)

# getDetailedVisits

`suspend fun getDetailedVisits(start: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, end: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, excludeTypes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitType`](../../mozilla.components.concept.storage/-visit-type/index.md)`>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`VisitInfo`](../../mozilla.components.concept.storage/-visit-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L64)

Overrides [HistoryStorage.getDetailedVisits](../../mozilla.components.concept.storage/-history-storage/get-detailed-visits.md)

Retrieves detailed information about all visits that occurred in the given time range.

### Parameters

`start` - The (inclusive) start time to bound the query.

`end` - The (inclusive) end time to bound the query.

`excludeTypes` - List of visit types to exclude.

**Return**
A list of all visits within the specified range, described by [VisitInfo](../../mozilla.components.concept.storage/-visit-info/index.md).

