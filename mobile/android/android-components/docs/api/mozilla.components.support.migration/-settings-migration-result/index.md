[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [SettingsMigrationResult](./index.md)

# SettingsMigrationResult

`sealed class SettingsMigrationResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecSettingsMigrator.kt#L104)

Result of Fennec settings migration.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `sealed class Failure : `[`SettingsMigrationResult`](./index.md)<br>Failed settings migrations. |
| [Success](-success/index.md) | `sealed class Success : `[`SettingsMigrationResult`](./index.md)<br>Successful setting migration. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `sealed class Failure : `[`SettingsMigrationResult`](./index.md)<br>Failed settings migrations. |
| [Success](-success/index.md) | `sealed class Success : `[`SettingsMigrationResult`](./index.md)<br>Successful setting migration. |
