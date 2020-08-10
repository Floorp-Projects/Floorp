[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [HistoryStorage](index.md) / [deleteVisitsBetween](./delete-visits-between.md)

# deleteVisitsBetween

`abstract suspend fun deleteVisitsBetween(startTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, endTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L110)

Remove history visits in an inclusive range from [startTime](delete-visits-between.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsBetween(kotlin.Long, kotlin.Long)/startTime) to [endTime](delete-visits-between.md#mozilla.components.concept.storage.HistoryStorage$deleteVisitsBetween(kotlin.Long, kotlin.Long)/endTime).

### Parameters

`startTime` - A unix timestamp, milliseconds.

`endTime` - A unix timestamp, milliseconds.