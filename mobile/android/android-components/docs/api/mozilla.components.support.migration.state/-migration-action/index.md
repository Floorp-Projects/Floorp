[android-components](../../index.md) / [mozilla.components.support.migration.state](../index.md) / [MigrationAction](./index.md)

# MigrationAction

`sealed class MigrationAction : `[`Action`](../../mozilla.components.lib.state/-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/state/MigrationAction.kt#L14)

Actions supported by the [MigrationStore](../-migration-store/index.md).

### Types

| Name | Summary |
|---|---|
| [Clear](-clear.md) | `object Clear : `[`MigrationAction`](./index.md)<br>Clear (or reset) the migration state. |
| [Completed](-completed.md) | `object Completed : `[`MigrationAction`](./index.md)<br>A migration was completed. |
| [MigrationRunResult](-migration-run-result/index.md) | `data class MigrationRunResult : `[`MigrationAction`](./index.md)<br>Set the result of a migration run. |
| [Started](-started.md) | `object Started : `[`MigrationAction`](./index.md)<br>A migration is needed and has been started. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Clear](-clear.md) | `object Clear : `[`MigrationAction`](./index.md)<br>Clear (or reset) the migration state. |
| [Completed](-completed.md) | `object Completed : `[`MigrationAction`](./index.md)<br>A migration was completed. |
| [MigrationRunResult](-migration-run-result/index.md) | `data class MigrationRunResult : `[`MigrationAction`](./index.md)<br>Set the result of a migration run. |
| [Started](-started.md) | `object Started : `[`MigrationAction`](./index.md)<br>A migration is needed and has been started. |
