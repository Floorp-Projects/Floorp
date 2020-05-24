[android-components](../index.md) / [mozilla.components.service.fxa.manager](./index.md)

## Package mozilla.components.service.fxa.manager

### Types

| Name | Summary |
|---|---|
| [AccountState](-account-state/index.md) | `enum class AccountState`<br>States of the [FxaAccountManager](-fxa-account-manager/index.md). |
| [FxaAccountManager](-fxa-account-manager/index.md) | `open class FxaAccountManager : `[`Closeable`](http://docs.oracle.com/javase/7/docs/api/java/io/Closeable.html)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`AccountObserver`](../mozilla.components.concept.sync/-account-observer/index.md)`>`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |
| [SignInWithShareableAccountResult](-sign-in-with-shareable-account-result/index.md) | `enum class SignInWithShareableAccountResult`<br>Describes a result of running [FxaAccountManager.signInWithShareableAccountAsync](-fxa-account-manager/sign-in-with-shareable-account-async.md). |
| [SyncEnginesStorage](-sync-engines-storage/index.md) | `class SyncEnginesStorage`<br>Storage layer for the enabled/disabled state of [SyncEngine](../mozilla.components.service.fxa/-sync-engine/index.md). |

### Properties

| Name | Summary |
|---|---|
| [SCOPE_PROFILE](-s-c-o-p-e_-p-r-o-f-i-l-e.md) | `const val SCOPE_PROFILE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SCOPE_SESSION](-s-c-o-p-e_-s-e-s-s-i-o-n.md) | `const val SCOPE_SESSION: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SCOPE_SYNC](-s-c-o-p-e_-s-y-n-c.md) | `const val SCOPE_SYNC: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
