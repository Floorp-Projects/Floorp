[android-components](../../index.md) / [mozilla.components.service.fxa.manager](../index.md) / [FxaAccountManager](./index.md)

# FxaAccountManager

`open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../../mozilla.components.concept.sync/-account-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L100)

An account manager which encapsulates various internal details of an account lifecycle and provides
an observer interface along with a public API for interacting with an account.
The internal state machine abstracts over state space as exposed by the fxaclient library, not
the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.

Class is 'open' to facilitate testing.

### Parameters

`context` - A [Context](https://developer.android.com/reference/android/content/Context.html) instance that's used for internal messaging and interacting with local storage.

`config` - A [Config](../../mozilla.components.service.fxa/-config.md) used for account initialization.

`scopes` - A list of scopes which will be requested during account authentication.

`deviceTuple` - A description of the current device (name, type, capabilities).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaAccountManager(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, config: `[`Config`](../../mozilla.components.service.fxa/-config.md)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, deviceTuple: `[`DeviceTuple`](../-device-tuple/index.md)`, syncManager: `[`SyncManager`](../../mozilla.components.concept.sync/-sync-manager/index.md)`? = null, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors
            .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob())`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |

### Functions

| Name | Summary |
|---|---|
| [accountNeedsReauth](account-needs-reauth.md) | `fun accountNeedsReauth(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if account needs to be re-authenticated via [beginAuthenticationAsync](begin-authentication-async.md). Most common reason for an account to need re-authentication is a password change. |
| [accountProfile](account-profile.md) | `fun accountProfile(): `[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?` |
| [authenticatedAccount](authenticated-account.md) | `fun authenticatedAccount(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?`<br>Main point for interaction with an [OAuthAccount](../../mozilla.components.concept.sync/-o-auth-account/index.md) instance. |
| [beginAuthenticationAsync](begin-authentication-async.md) | `fun beginAuthenticationAsync(pairingUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createAccount](create-account.md) | `open fun createAccount(config: `[`Config`](../../mozilla.components.service.fxa/-config.md)`): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md) |
| [finishAuthenticationAsync](finish-authentication-async.md) | `fun finishAuthenticationAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [getAccountStorage](get-account-storage.md) | `open fun getAccountStorage(): `[`AccountStorage`](../../mozilla.components.service.fxa/-account-storage/index.md) |
| [initAsync](init-async.md) | `fun initAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Call this after registering your observers, and before interacting with this class. |
| [logoutAsync](logout-async.md) | `fun logoutAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [registerForDeviceEvents](register-for-device-events.md) | `fun registerForDeviceEvents(observer: `[`DeviceEventsObserver`](../../mozilla.components.concept.sync/-device-events-observer/index.md)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [updateProfileAsync](update-profile-async.md) | `fun updateProfileAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
