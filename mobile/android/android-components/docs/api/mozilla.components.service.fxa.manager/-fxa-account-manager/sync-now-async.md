[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](index.md) / [syncNowAsync](./sync-now-async.md)

# syncNowAsync

`fun syncNowAsync(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, debounce: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L327)

Request an immediate synchronization, as configured according to [syncConfig](#).

### Parameters

`startup` - Boolean flag indicating if sync is being requested in a startup situation.

`debounce` - Boolean flag indicating if this sync may be debounced (in case another sync executed recently).