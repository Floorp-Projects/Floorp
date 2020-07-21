[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [SyncEnginesStorage](./index.md)

# SyncEnginesStorage

`class SyncEnginesStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/SyncEnginesStorage.kt#L14)

Storage layer for the enabled/disabled state of [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncEnginesStorage(context: <ERROR CLASS>)`<br>Storage layer for the enabled/disabled state of [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getStatus](get-status.md) | `fun getStatus(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`, `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` |
| [setStatus](set-status.md) | `fun setStatus(engine: `[`SyncEngine`](../../mozilla.components.service.fxa/-sync-engine/index.md)`, status: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Update enabled/disabled state of [engine](set-status.md#mozilla.components.service.fxa.manager.SyncEnginesStorage$setStatus(mozilla.components.service.fxa.SyncEngine, kotlin.Boolean)/engine). |

### Companion Object Properties

| Name | Summary |
|---|---|
| [SYNC_ENGINES_KEY](-s-y-n-c_-e-n-g-i-n-e-s_-k-e-y.md) | `const val SYNC_ENGINES_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
