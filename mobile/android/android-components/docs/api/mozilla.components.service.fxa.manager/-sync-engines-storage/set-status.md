[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [SyncEnginesStorage](index.md) / [setStatus](./set-status.md)

# setStatus

`fun setStatus(engine: `[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`, status: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/SyncEnginesStorage.kt#L45)

Update enabled/disabled state of [engine](set-status.md#mozilla.components.service.fxa.manager.SyncEnginesStorage$setStatus(mozilla.components.service.fxa.SyncEngine, kotlin.Boolean)/engine).

### Parameters

`engine` - A [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md) for which to update state.

`status` - New state.