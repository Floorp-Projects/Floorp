[android-components](../index.md) / [mozilla.components.feature.sync](./index.md)

## Package mozilla.components.feature.sync

### Types

| Name | Summary |
|---|---|
| [AuthExceptionType](-auth-exception-type/index.md) | `enum class AuthExceptionType`<br>An auth-related exception type, for use with [AuthException](-auth-exception/index.md). |
| [FirefoxSyncFeature](-firefox-sync-feature/index.md) | `class FirefoxSyncFeature<AuthType> : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>`<br>A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../mozilla.components.concept.storage/-syncable-store/index.md) which all share a common [AuthType](-firefox-sync-feature/index.md#AuthType). |
| [FxaAuthInfo](-fxa-auth-info/index.md) | `data class FxaAuthInfo`<br>A Firefox Sync friendly auth object which can be obtained from [FirefoxAccount](../mozilla.components.service.fxa/-firefox-account/index.md). |
| [StoreSyncStatus](-store-sync-status/index.md) | `data class StoreSyncStatus` |
| [SyncStatusObserver](-sync-status-observer/index.md) | `interface SyncStatusObserver`<br>An interface for consumers that wish to observer "sync lifecycle" events. |

### Exceptions

| Name | Summary |
|---|---|
| [AuthException](-auth-exception/index.md) | `class AuthException : `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)<br>An exception which may happen while obtaining auth information using [FirefoxAccount](../mozilla.components.service.fxa/-firefox-account/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [SyncResult](-sync-result.md) | `typealias SyncResult = `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`StoreSyncStatus`](-store-sync-status/index.md)`>`<br>A set of results of running a sync operation for all configured stores. |

### Properties

| Name | Summary |
|---|---|
| [registry](registry.md) | `val registry: `[`ObserverRegistry`](../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>` |
