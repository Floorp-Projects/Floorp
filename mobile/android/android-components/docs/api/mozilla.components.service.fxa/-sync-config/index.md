[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncConfig](./index.md)

# SyncConfig

`data class SyncConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L40)

Configuration for sync.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncConfig(supportedEngines: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`SyncEngine`](../-sync-engine/index.md)`>, syncPeriodInMinutes: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null)`<br>Configuration for sync. |

### Properties

| Name | Summary |
|---|---|
| [supportedEngines](supported-engines.md) | `val supportedEngines: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`SyncEngine`](../-sync-engine/index.md)`>`<br>A set of supported sync engines, exposed via [GlobalSyncableStoreProvider](../../mozilla.components.service.fxa.sync/-global-syncable-store-provider/index.md). |
| [syncPeriodInMinutes](sync-period-in-minutes.md) | `val syncPeriodInMinutes: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Optional, how frequently periodic sync should happen. If this is `null`, periodic syncing will be disabled. |
