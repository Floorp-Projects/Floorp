[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](./index.md)

# FxaAccountManager

`open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../../mozilla.components.concept.sync/-account-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L101)

An account manager which encapsulates various internal details of an account lifecycle and provides
an observer interface along with a public API for interacting with an account.
The internal state machine abstracts over state space as exposed by the fxaclient library, not
the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.

Class is 'open' to facilitate testing.

### Parameters

`context` - A [Context](#) instance that's used for internal messaging and interacting with local storage.

`serverConfig` - A [ServerConfig](../../mozilla.components.service.fxa/-server-config.md) used for account initialization.

`deviceConfig` - A description of the current device (name, type, capabilities).

`syncConfig` - Optional, initial sync behaviour configuration. Sync will be disabled if this is `null`.

`applicationScopes` - A set of scopes which will be requested during account authentication.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaAccountManager(context: <ERROR CLASS>, serverConfig: `[`ServerConfig`](../../mozilla.components.service.fxa/-server-config.md)`, deviceConfig: `[`DeviceConfig`](../../mozilla.components.service.fxa/-device-config/index.md)`, syncConfig: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`?, applicationScopes: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptySet(), coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors
        .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob())`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |

### Functions

| Name | Summary |
|---|---|
| [accountNeedsReauth](account-needs-reauth.md) | `fun accountNeedsReauth(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if account needs to be re-authenticated via [beginAuthenticationAsync](begin-authentication-async.md). Most common reason for an account to need re-authentication is a password change. |
| [accountProfile](account-profile.md) | `fun accountProfile(): `[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?` |
| [authenticatedAccount](authenticated-account.md) | `fun authenticatedAccount(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?`<br>Main point for interaction with an [OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md) instance. |
| [beginAuthenticationAsync](begin-authentication-async.md) | `fun beginAuthenticationAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createAccount](create-account.md) | `open fun createAccount(config: `[`ServerConfig`](../../mozilla.components.service.fxa/-server-config.md)`): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md) |
| [createSyncManager](create-sync-manager.md) | `open fun createSyncManager(config: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`): `[`SyncManager`](../../mozilla.components.service.fxa.sync/-sync-manager/index.md) |
| [finishAuthenticationAsync](finish-authentication-async.md) | `fun finishAuthenticationAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [getAccountStorage](get-account-storage.md) | `open fun getAccountStorage(): `[`AccountStorage`](../../mozilla.components.service.fxa/-account-storage/index.md) |
| [initAsync](init-async.md) | `fun initAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Call this after registering your observers, and before interacting with this class. |
| [isSyncActive](is-sync-active.md) | `fun isSyncActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if sync is currently running. |
| [logoutAsync](logout-async.md) | `fun logoutAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [registerForDeviceEvents](register-for-device-events.md) | `fun registerForDeviceEvents(observer: `[`DeviceEventsObserver`](../../mozilla.components.concept.sync/-device-events-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [registerForSyncEvents](register-for-sync-events.md) | `fun registerForSyncEvents(observer: `[`SyncStatusObserver`](../../mozilla.components.service.fxa.sync/-sync-status-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setSyncConfigAsync](set-sync-config-async.md) | `fun setSyncConfigAsync(config: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Allows setting a new [SyncConfig](../../mozilla.components.service.fxa/-sync-config/index.md), changing sync behaviour. |
| [shareableAccounts](shareable-accounts.md) | `fun shareableAccounts(context: <ERROR CLASS>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ShareableAccount`](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md)`>`<br>Queries trusted FxA Auth providers available on the device, returning a list of [ShareableAccount](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md) in an order of preference. Any of the returned [ShareableAccount](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md) may be used with [signInWithShareableAccountAsync](sign-in-with-shareable-account-async.md) to sign-in into an FxA account without any required user input. |
| [signInWithShareableAccountAsync](sign-in-with-shareable-account-async.md) | `fun signInWithShareableAccountAsync(fromAccount: `[`ShareableAccount`](../../mozilla.components.service.fxa.sharing/-shareable-account/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Uses a provided [fromAccount](sign-in-with-shareable-account-async.md#mozilla.components.service.fxa.manager.FxaAccountManager$signInWithShareableAccountAsync(mozilla.components.service.fxa.sharing.ShareableAccount)/fromAccount) to sign-in into a corresponding FxA account without any required user input. Once sign-in completes, any registered [AccountObserver.onAuthenticated](../../mozilla.components.concept.sync/-account-observer/on-authenticated.md) listeners will be notified and [authenticatedAccount](authenticated-account.md) will refer to the new account. This may fail in case of network errors, or if provided credentials are not valid. |
| [syncNowAsync](sync-now-async.md) | `fun syncNowAsync(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, debounce: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Request an immediate synchronization, as configured according to [syncConfig](#). |
| [updateProfileAsync](update-profile-async.md) | `fun updateProfileAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
