[android-components](../index.md) / [mozilla.components.concept.sync](./index.md)

## Package mozilla.components.concept.sync

### Types

| Name | Summary |
|---|---|
| [AccessTokenInfo](-access-token-info/index.md) | `data class AccessTokenInfo`<br>The result of authentication with FxA via an OAuth flow. |
| [AccountObserver](-account-observer/index.md) | `interface AccountObserver`<br>Observer interface which lets its users monitor account state changes and major events. |
| [AuthExceptionType](-auth-exception-type/index.md) | `enum class AuthExceptionType`<br>An auth-related exception type, for use with [AuthException](-auth-exception/index.md). |
| [AuthInfo](-auth-info/index.md) | `data class AuthInfo`<br>A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](-o-auth-account/index.md). |
| [Avatar](-avatar/index.md) | `data class Avatar` |
| [OAuthAccount](-o-auth-account/index.md) | `interface OAuthAccount : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>Facilitates testing consumers of FirefoxAccount. |
| [OAuthScopedKey](-o-auth-scoped-key/index.md) | `data class OAuthScopedKey`<br>Scoped key data. |
| [Profile](-profile/index.md) | `data class Profile` |
| [StoreSyncStatus](-store-sync-status/index.md) | `data class StoreSyncStatus` |
| [SyncManager](-sync-manager/index.md) | `interface SyncManager : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](-sync-status-observer/index.md)`>`<br>Describes a "sync" entry point for an application. |
| [SyncStatus](-sync-status/index.md) | `sealed class SyncStatus`<br>Results of running a sync via [SyncableStore.sync](-syncable-store/sync.md). |
| [SyncStatusObserver](-sync-status-observer/index.md) | `interface SyncStatusObserver`<br>An interface for consumers that wish to observer "sync lifecycle" events. |
| [SyncableStore](-syncable-store/index.md) | `interface SyncableStore`<br>Describes a "sync" entry point for a storage layer. |

### Exceptions

| Name | Summary |
|---|---|
| [AuthException](-auth-exception/index.md) | `class AuthException : `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html)<br>An exception which may happen while obtaining auth information using [OAuthAccount](-o-auth-account/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [SyncResult](-sync-result.md) | `typealias SyncResult = `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`StoreSyncStatus`](-store-sync-status/index.md)`>`<br>A set of results of running a sync operation for multiple instances of [SyncableStore](-syncable-store/index.md). |
