[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncReason](./index.md)

# SyncReason

`sealed class SyncReason` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L21)

A collection of objects describing a reason for running a sync.

### Types

| Name | Summary |
|---|---|
| [EngineChange](-engine-change.md) | `object EngineChange : `[`SyncReason`](./index.md)<br>User changed enabled/disabled state of one or more [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md)s. |
| [Startup](-startup.md) | `object Startup : `[`SyncReason`](./index.md)<br>Application is starting up, and wants to sync data. |
| [User](-user.md) | `object User : `[`SyncReason`](./index.md)<br>User is requesting a sync (e.g. pressed a "sync now" button). |

### Inheritors

| Name | Summary |
|---|---|
| [EngineChange](-engine-change.md) | `object EngineChange : `[`SyncReason`](./index.md)<br>User changed enabled/disabled state of one or more [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md)s. |
| [Startup](-startup.md) | `object Startup : `[`SyncReason`](./index.md)<br>Application is starting up, and wants to sync data. |
| [User](-user.md) | `object User : `[`SyncReason`](./index.md)<br>User is requesting a sync (e.g. pressed a "sync now" button). |
