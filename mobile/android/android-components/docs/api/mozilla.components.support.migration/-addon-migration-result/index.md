[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [AddonMigrationResult](./index.md)

# AddonMigrationResult

`sealed class AddonMigrationResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/AddonMigration.kt#L44)

Result of an add-on migration.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `sealed class Failure : `[`AddonMigrationResult`](./index.md)<br>Failure variants of an add-on migration. |
| [Success](-success/index.md) | `sealed class Success : `[`AddonMigrationResult`](./index.md)<br>Success variants of an add-on migration. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure.md) | `sealed class Failure : `[`AddonMigrationResult`](./index.md)<br>Failure variants of an add-on migration. |
| [Success](-success/index.md) | `sealed class Success : `[`AddonMigrationResult`](./index.md)<br>Success variants of an add-on migration. |
