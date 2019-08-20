[android-components](../index.md) / [mozilla.components.service.fxa.manager](./index.md)

## Package mozilla.components.service.fxa.manager

### Types

| Name | Summary |
|---|---|
| [AccountState](-account-state/index.md) | `enum class AccountState`<br>States of the [FxaAccountManager](-fxa-account-manager/index.md). |
| [AuthErrorObserver](-auth-error-observer/index.md) | `interface AuthErrorObserver` |
| [FxaAccountManager](-fxa-account-manager/index.md) | `open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../mozilla.components.concept.sync/-account-observer/index.md)`>`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |

### Properties

| Name | Summary |
|---|---|
| [SCOPE_PROFILE](-s-c-o-p-e_-p-r-o-f-i-l-e.md) | `const val SCOPE_PROFILE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SCOPE_SYNC](-s-c-o-p-e_-s-y-n-c.md) | `const val SCOPE_SYNC: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [authErrorRegistry](auth-error-registry.md) | `val authErrorRegistry: `[`ObserverRegistry`](../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`AuthErrorObserver`](-auth-error-observer/index.md)`>`<br>A global registry for propagating [AuthException](../mozilla.components.concept.sync/-auth-exception/index.md) errors. Components such as [SyncManager](../mozilla.components.service.fxa.sync/-sync-manager/index.md) and [FxaDeviceRefreshManager](#) may encounter authentication problems during their normal operation, and this registry is how they inform [FxaAccountManager](-fxa-account-manager/index.md) that these errors happened. |
