[android-components](../index.md) / [mozilla.components.concept.sync](./index.md)

## Package mozilla.components.concept.sync

### Types

| Name | Summary |
|---|---|
| [AccessTokenInfo](-access-token-info/index.md) | `data class AccessTokenInfo`<br>The result of authentication with FxA via an OAuth flow. |
| [AccountObserver](-account-observer/index.md) | `interface AccountObserver`<br>Observer interface which lets its users monitor account state changes and major events. |
| [AuthExceptionType](-auth-exception-type/index.md) | `enum class AuthExceptionType`<br>An auth-related exception type, for use with [AuthException](-auth-exception/index.md). |
| [AuthFlowUrl](-auth-flow-url/index.md) | `data class AuthFlowUrl`<br>An object that represents a login flow initiated by [OAuthAccount](-o-auth-account/index.md). |
| [AuthType](-auth-type/index.md) | `sealed class AuthType` |
| [Avatar](-avatar/index.md) | `data class Avatar` |
| [ConstellationState](-constellation-state/index.md) | `data class ConstellationState`<br>Describes current device and other devices in the constellation. |
| [Device](-device/index.md) | `data class Device`<br>Describes a device in the [DeviceConstellation](-device-constellation/index.md). |
| [DeviceCapability](-device-capability/index.md) | `enum class DeviceCapability`<br>Capabilities that a [Device](-device/index.md) may have. |
| [DeviceConstellation](-device-constellation/index.md) | `interface DeviceConstellation : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`DeviceEventsObserver`](-device-events-observer/index.md)`>`<br>Describes available interactions with the current device and other devices associated with an [OAuthAccount](-o-auth-account/index.md). |
| [DeviceConstellationObserver](-device-constellation-observer/index.md) | `interface DeviceConstellationObserver`<br>Allows monitoring constellation state. |
| [DeviceEvent](-device-event/index.md) | `sealed class DeviceEvent`<br>Incoming device events, targeted at the current device. |
| [DeviceEventOutgoing](-device-event-outgoing/index.md) | `sealed class DeviceEventOutgoing`<br>Outgoing device events, targeted at other devices. |
| [DeviceEventsObserver](-device-events-observer/index.md) | `interface DeviceEventsObserver`<br>Allows monitoring events targeted at the current device. |
| [DevicePushSubscription](-device-push-subscription/index.md) | `data class DevicePushSubscription`<br>Describes an Autopush-compatible push channel subscription. |
| [DeviceType](-device-type/index.md) | `enum class DeviceType`<br>Describes a type of the physical device in the constellation. |
| [OAuthAccount](-o-auth-account/index.md) | `interface OAuthAccount : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>Facilitates testing consumers of FirefoxAccount. |
| [OAuthScopedKey](-o-auth-scoped-key/index.md) | `data class OAuthScopedKey`<br>Scoped key data. |
| [Profile](-profile/index.md) | `data class Profile` |
| [StatePersistenceCallback](-state-persistence-callback/index.md) | `interface StatePersistenceCallback`<br>Describes a delegate object that is used by [OAuthAccount](-o-auth-account/index.md) to persist its internal state as it changes. |
| [StoreSyncStatus](-store-sync-status/index.md) | `data class StoreSyncStatus` |
| [SyncAuthInfo](-sync-auth-info/index.md) | `data class SyncAuthInfo`<br>A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](-o-auth-account/index.md). |
| [SyncStatus](-sync-status/index.md) | `sealed class SyncStatus`<br>Results of running a sync via [SyncableStore.sync](-syncable-store/sync.md). |
| [SyncableStore](-syncable-store/index.md) | `interface SyncableStore`<br>Describes a "sync" entry point for a storage layer. |
| [TabData](-tab-data/index.md) | `data class TabData` |

### Exceptions

| Name | Summary |
|---|---|
| [AuthException](-auth-exception/index.md) | `class AuthException : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)<br>An exception which may happen while obtaining auth information using [OAuthAccount](-o-auth-account/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [SyncResult](-sync-result.md) | `typealias SyncResult = `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`StoreSyncStatus`](-store-sync-status/index.md)`>`<br>A set of results of running a sync operation for multiple instances of [SyncableStore](-syncable-store/index.md). |
