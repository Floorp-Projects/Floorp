[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [deleteVisit](./delete-visit.md)

# deleteVisit

`abstract suspend fun deleteVisit(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L114)

Remove a specific visit for a given [url](delete-visit.md#mozilla.components.concept.storage.HistoryStorage$deleteVisit(kotlin.String, kotlin.Long)/url).

### Parameters

`url` - A page URL for which to remove a visit.

`timestamp` - A unix timestamp, milliseconds, of a visit to be removed.