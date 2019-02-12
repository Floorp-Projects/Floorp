[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaAccountManager](./index.md)

# FxaAccountManager

`open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../-account-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaAccountManager.kt#L130)

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
| [&lt;init&gt;](-init-.md) | `FxaAccountManager(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, config: `[`Config`](../-config.md)`, scopes: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, accountStorage: `[`AccountStorage`](../-account-storage/index.md)` = SharedPrefAccountStorage(context))`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |

### Functions

| Name | Summary |
|---|---|
| [accountProfile](account-profile.md) | `fun accountProfile(): `[`Profile`](../-profile/index.md)`?` |
| [authenticatedAccount](authenticated-account.md) | `fun authenticatedAccount(): `[`FirefoxAccountShaped`](../-firefox-account-shaped/index.md)`?` |
| [beginAuthentication](begin-authentication.md) | `fun beginAuthentication(): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [createAccount](create-account.md) | `open fun createAccount(config: `[`Config`](../-config.md)`): `[`FirefoxAccountShaped`](../-firefox-account-shaped/index.md) |
| [finishAuthentication](finish-authentication.md) | `fun finishAuthentication(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [init](init.md) | `fun init(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Call this after registering your observers, and before interacting with this class. |
| [logout](logout.md) | `fun logout(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
| [updateProfile](update-profile.md) | `fun updateProfile(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` |
