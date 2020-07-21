[android-components](../../../../index.md) / [mozilla.components.support.migration](../../../index.md) / [FxaMigrationResult](../../index.md) / [Success](../index.md) / [UnauthenticatedAccount](./index.md)

# UnauthenticatedAccount

`data class UnauthenticatedAccount : `[`Success`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L64)

Encountered a Fennec auth state that can't be used to automatically log-in.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UnauthenticatedAccount(email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Encountered a Fennec auth state that can't be used to automatically log-in. |

### Properties

| Name | Summary |
|---|---|
| [email](email.md) | `val email: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [stateLabel](state-label.md) | `val stateLabel: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
