[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncConfig](./index.md)

# SyncConfig

`data class SyncConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L40)

Configuration for sync.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncConfig(syncableStores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, syncPeriodInMinutes: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null)`<br>Configuration for sync. |

### Properties

| Name | Summary |
|---|---|
| [syncPeriodInMinutes](sync-period-in-minutes.md) | `val syncPeriodInMinutes: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>Optional, how frequently periodic sync should happen. If this is `null`, periodic syncing will be disabled. |
| [syncableStores](syncable-stores.md) | `val syncableStores: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>A set of store names to sync, exposed via [GlobalSyncableStoreProvider](../../mozilla.components.service.fxa.sync/-global-syncable-store-provider/index.md). |
