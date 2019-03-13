[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncManager](index.md) / [syncNow](./sync-now.md)

# syncNow

`abstract fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L69)

Request an immediate synchronization of all configured stores.

### Parameters

`startup` - Boolean flag indicating if sync is being requested in a startup situation.