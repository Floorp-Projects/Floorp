[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FxaMigrationResult](./index.md)

# FxaMigrationResult

`sealed class FxaMigrationResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecFxaMigration.kt#L51)

Result of an FxA migration.

Note that toString implementations are overridden to help avoid accidental logging of PII.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `sealed class Failure : `[`FxaMigrationResult`](./index.md)<br>Failure variants of an FxA migration. |
| [Success](-success/index.md) | `sealed class Success : `[`FxaMigrationResult`](./index.md)<br>Success variants of an FxA migration. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `sealed class Failure : `[`FxaMigrationResult`](./index.md)<br>Failure variants of an FxA migration. |
| [Success](-success/index.md) | `sealed class Success : `[`FxaMigrationResult`](./index.md)<br>Success variants of an FxA migration. |
