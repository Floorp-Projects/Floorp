[android-components](../index.md) / [mozilla.components.service.fxa](./index.md)

## Package mozilla.components.service.fxa

### Types

| Name | Summary |
|---|---|
| [AccountState](-account-state/index.md) | `enum class AccountState` |
| [AccountStorage](-account-storage/index.md) | `interface AccountStorage` |
| [FirefoxAccount](-firefox-account/index.md) | `class FirefoxAccount : `[`OAuthAccount`](../mozilla.components.concept.sync/-o-auth-account/index.md)<br>FirefoxAccount represents the authentication state of a client. |
| [FxaAccountManager](-fxa-account-manager/index.md) | `open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../mozilla.components.concept.sync/-account-observer/index.md)`>`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |
| [SharedPrefAccountStorage](-shared-pref-account-storage/index.md) | `class SharedPrefAccountStorage : `[`AccountStorage`](-account-storage/index.md) |

### Exceptions

| Name | Summary |
|---|---|
| [FailedToLoadAccountException](-failed-to-load-account-exception/index.md) | `class FailedToLoadAccountException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Propagated via [AccountObserver.onError](../mozilla.components.concept.sync/-account-observer/on-error.md) if we fail to load a locally stored account during initialization. No action is necessary from consumers. Account state has been re-initialized. |

### Type Aliases

| Name | Summary |
|---|---|
| [Config](-config.md) | `typealias Config = Config` |
| [FxaException](-fxa-exception.md) | `typealias FxaException = FxaException`<br>High-level exception class for the exceptions thrown in the Rust library. |
| [FxaNetworkException](-fxa-network-exception.md) | `typealias FxaNetworkException = Network`<br>Thrown on a network error. |
| [FxaPanicException](-fxa-panic-exception.md) | `typealias FxaPanicException = Panic`<br>Thrown when the Rust library hits an assertion or panic (this is always a bug). |
| [FxaUnauthorizedException](-fxa-unauthorized-exception.md) | `typealias FxaUnauthorizedException = Unauthorized`<br>Thrown when the operation requires additional authorization. |
| [FxaUnspecifiedException](-fxa-unspecified-exception.md) | `typealias FxaUnspecifiedException = Unspecified`<br>Thrown when the Rust library hits an unexpected error that isn't a panic. This may indicate library misuse, network errors, etc. |
| [PersistCallback](-persist-callback.md) | `typealias PersistCallback = PersistCallback` |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [mozilla.appservices.fxaclient.AccessTokenInfo](mozilla.appservices.fxaclient.-access-token-info/index.md) |  |
| [mozilla.appservices.fxaclient.Profile](mozilla.appservices.fxaclient.-profile/index.md) |  |
| [mozilla.appservices.fxaclient.ScopedKey](mozilla.appservices.fxaclient.-scoped-key/index.md) |  |

### Properties

| Name | Summary |
|---|---|
| [FXA_STATE_KEY](-f-x-a_-s-t-a-t-e_-k-e-y.md) | `const val FXA_STATE_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FXA_STATE_PREFS_KEY](-f-x-a_-s-t-a-t-e_-p-r-e-f-s_-k-e-y.md) | `const val FXA_STATE_PREFS_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
