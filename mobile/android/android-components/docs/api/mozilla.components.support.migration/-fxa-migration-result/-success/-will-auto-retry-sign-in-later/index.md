[android-components](../../../../index.md) / [mozilla.components.support.migration](../../../index.md) / [FxaMigrationResult](../../index.md) / [Success](../index.md) / [WillAutoRetrySignInLater](./index.md)

# WillAutoRetrySignInLater

`data class WillAutoRetrySignInLater : `[`Success`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L82)

Sign-in attempt encountered a recoverable problem, and a retry will be performed later.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WillAutoRetrySignInLater(email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Sign-in attempt encountered a recoverable problem, and a retry will be performed later. |

### Properties

| Name | Summary |
|---|---|
| [email](email.md) | `val email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [stateLabel](state-label.md) | `val stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
