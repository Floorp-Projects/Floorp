[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncManager](./index.md)

# SyncManager

`abstract class SyncManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L106)

A base sync manager implementation.

### Parameters

`syncConfig` - A [SyncConfig](../../mozilla.components.service.fxa/-sync-config/index.md) object describing how sync should behave.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncManager(syncConfig: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`)`<br>A base sync manager implementation. |

### Properties

| Name | Summary |
|---|---|
| [logger](logger.md) | `open val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [SYNC_PERIOD_UNIT](-s-y-n-c_-p-e-r-i-o-d_-u-n-i-t.md) | `val SYNC_PERIOD_UNIT: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html) |
