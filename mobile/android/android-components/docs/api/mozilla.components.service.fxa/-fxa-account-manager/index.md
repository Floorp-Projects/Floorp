[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaAccountManager](./index.md)

# FxaAccountManager

`open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../../mozilla.components.concept.sync/-account-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaAccountManager.kt#L106)

An account manager which encapsulates various internal details of an account lifecycle and provides
an observer interface along with a public API for interacting with an account.
The internal state machine abstracts over state space as exposed by the fxaclient library, not
the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.

Class is 'open' to facilitate testing.

### Parameters

`context` - A [Context](https://developer.android.com/reference/android/content/Context.html) instance that's used for internal messaging and interacting with local storage.

`config` - A [Config](../-config.md) used for account initialization.

`scopes` - A list of scopes which will be requested during account authentication.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaAccountManager(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, config: `[`Config`](../-config.md)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, syncManager: `[`SyncManager`](../../mozilla.components.concept.sync/-sync-manager/index.md)`? = null)`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |

### Functions

| Name | Summary |
|---|---|
| [accountProfile](account-profile.md) | `fun accountProfile(): `[`Profile`](../../mozilla.components.concept.sync/-profile/index.md)`?` |
| [authenticatedAccount](authenticated-account.md) | `fun authenticatedAccount(): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md)`?` |
| [beginAuthenticationAsync](begin-authentication-async.md) | `fun beginAuthenticationAsync(): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createAccount](create-account.md) | `open fun createAccount(config: `[`Config`](../-config.md)`): `[`OAuthAccount`](../../mozilla.components.concept.sync/-o-auth-account/index.md) |
| [finishAuthenticationAsync](finish-authentication-async.md) | `fun finishAuthenticationAsync(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [getAccountStorage](get-account-storage.md) | `open fun getAccountStorage(): `[`AccountStorage`](../-account-storage/index.md) |
| [initAsync](init-async.md) | `fun initAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Call this after registering your observers, and before interacting with this class. |
| [logoutAsync](logout-async.md) | `fun logoutAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [updateProfileAsync](update-profile-async.md) | `fun updateProfileAsync(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
