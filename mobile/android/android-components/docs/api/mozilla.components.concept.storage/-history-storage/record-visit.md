[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [recordVisit](./record-visit.md)

# recordVisit

`abstract suspend fun recordVisit(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, visit: `[`PageVisit`](../-page-visit/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L17)

Records a visit to a page.

### Parameters

`uri` - of the page which was visited.

`visit` - Information about the visit; see [PageVisit](../-page-visit/index.md).