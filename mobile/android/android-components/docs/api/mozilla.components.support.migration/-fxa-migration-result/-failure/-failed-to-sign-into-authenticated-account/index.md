[android-components](../../../../index.md) / [mozilla.components.support.migration](../../../index.md) / [FxaMigrationResult](../../index.md) / [Failure](../index.md) / [FailedToSignIntoAuthenticatedAccount](./index.md)

# FailedToSignIntoAuthenticatedAccount

`data class FailedToSignIntoAuthenticatedAccount : `[`Failure`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L119)

Failed to sign in into an authenticated account. Currently, this could be either due to network failures,
invalid credentials, or server-side issues.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FailedToSignIntoAuthenticatedAccount(email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Failed to sign in into an authenticated account. Currently, this could be either due to network failures, invalid credentials, or server-side issues. |

### Properties

| Name | Summary |
|---|---|
| [email](email.md) | `val email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [stateLabel](state-label.md) | `val stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
