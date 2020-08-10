[android-components](../../../../index.md) / [mozilla.components.support.migration](../../../index.md) / [FxaMigrationResult](../../index.md) / [Failure](../index.md) / [UnsupportedVersions](./index.md)

# UnsupportedVersions

`data class UnsupportedVersions : `[`Failure`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L105)

Encountered an unsupported version of Fennec's auth state.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UnsupportedVersions(accountVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, pickleVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, stateVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null)`<br>Encountered an unsupported version of Fennec's auth state. |

### Properties

| Name | Summary |
|---|---|
| [accountVersion](account-version.md) | `val accountVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [pickleVersion](pickle-version.md) | `val pickleVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [stateVersion](state-version.md) | `val stateVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |

### Functions

| Name | Summary |
|---|---|
| [toString](to-string.md) | `fun toString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
