[android-components](../../index.md) / [mozilla.components.browser.storage.sync](../index.md) / [PlacesHistoryStorage](index.md) / [recordVisit](./record-visit.md)

# recordVisit

`open suspend fun recordVisit(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visitType: `[`VisitType`](../../mozilla.components.concept.storage/-visit-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/storage-sync/src/main/java/mozilla/components/browser/storage/sync/PlacesHistoryStorage.kt#L33)

Overrides [HistoryStorage.recordVisit](../../mozilla.components.concept.storage/-history-storage/record-visit.md)

Records a visit to a page.

### Parameters

`uri` - of the page which was visited.

`visitType` - type of the visit, see [VisitType](../../mozilla.components.concept.storage/-visit-type/index.md).