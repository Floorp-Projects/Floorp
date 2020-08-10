[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [deleteVisitsSince](./delete-visits-since.md)

# deleteVisitsSince

`abstract suspend fun deleteVisitsSince(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L103)

Remove history visits in an inclusive range from [since](delete-visits-since.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsSince(kotlin.Long)/since) to now.

### Parameters

`since` - A unix timestamp, milliseconds.