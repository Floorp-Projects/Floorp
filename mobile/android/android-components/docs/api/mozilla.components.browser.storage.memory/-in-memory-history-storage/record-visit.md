[android-components](../../index.md) / [mozilla.components.browser.storage.memory](../index.md) / [InMemoryHistoryStorage](index.md) / [recordVisit](./record-visit.md)

# recordVisit

`suspend fun recordVisit(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visitType: `[`VisitType`](../../mozilla.components.concept.storage/-visit-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-memory/src/main/java/mozilla/components/browser/storage/memory/InMemoryHistoryStorage.kt#L31)

Overrides [HistoryStorage.recordVisit](../../mozilla.components.concept.storage/-history-storage/record-visit.md)

Records a visit to a page.

### Parameters

`uri` - of the page which was visited.

`visitType` - type of the visit, see [VisitType](../../mozilla.components.concept.storage/-visit-type/index.md).