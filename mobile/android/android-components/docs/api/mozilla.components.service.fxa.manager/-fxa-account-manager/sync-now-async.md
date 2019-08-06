[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [syncNowAsync](./sync-now-async.md)

# syncNowAsync

`fun syncNowAsync(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L325)

Request an immediate synchronization, as configured according to [syncConfig](#).

### Parameters

`startup` - Boolean flag indicating if sync is being requested in a startup situation.