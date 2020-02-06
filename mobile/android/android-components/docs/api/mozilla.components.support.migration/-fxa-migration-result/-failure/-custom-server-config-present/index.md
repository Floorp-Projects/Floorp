[android-components](../../../../index.md) / [mozilla.components.support.migration](../../../index.md) / [FxaMigrationResult](../../index.md) / [Failure](../index.md) / [CustomServerConfigPresent](./index.md)

# CustomServerConfigPresent

`data class CustomServerConfigPresent : `[`Failure`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L128)

Encountered a Fennec with customized token/idp server endpoints.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomServerConfigPresent(customTokenServer: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, customIdpServer: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Encountered a Fennec with customized token/idp server endpoints. |

### Properties

| Name | Summary |
|---|---|
| [customIdpServer](custom-idp-server.md) | `val customIdpServer: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [customTokenServer](custom-token-server.md) | `val customTokenServer: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
