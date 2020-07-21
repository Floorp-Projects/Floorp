[android-components](../../index.md) / [mozilla.components.support.migration.state](../index.md) / [MigrationState](./index.md)

# MigrationState

`data class MigrationState : `[`State`](../../mozilla.components.lib.state/-state.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/state/MigrationState.kt#L13)

Value type that represents the state of the migration.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MigrationState(progress: `[`MigrationProgress`](../-migration-progress/index.md)` = MigrationProgress.NONE, results: `[`MigrationResults`](../../mozilla.components.support.migration/-migration-results.md)`? = null)`<br>Value type that represents the state of the migration. |

### Properties

| Name | Summary |
|---|---|
| [progress](progress.md) | `val progress: `[`MigrationProgress`](../-migration-progress/index.md) |
| [results](results.md) | `val results: `[`MigrationResults`](../../mozilla.components.support.migration/-migration-results.md)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
