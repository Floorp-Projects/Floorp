[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [MigrationStateListener](./index.md)

# MigrationStateListener

`interface MigrationStateListener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/AbstractMigrationProgressActivity.kt#L46)

Interface to be implemented by classes that want to observe the migration state changes.

### Functions

| Name | Summary |
|---|---|
| [onMigrationCompleted](on-migration-completed.md) | `abstract fun onMigrationCompleted(results: `[`MigrationResults`](../-migration-results.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when the migration is complete. |
| [onMigrationStateChanged](on-migration-state-changed.md) | `abstract fun onMigrationStateChanged(progress: `[`MigrationProgress`](../../mozilla.components.support.migration.state/-migration-progress/index.md)`, results: `[`MigrationResults`](../-migration-results.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked on a migration state change. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AbstractMigrationProgressActivity](../-abstract-migration-progress-activity/index.md) | `abstract class AbstractMigrationProgressActivity : AppCompatActivity, `[`MigrationStateListener`](./index.md)<br>An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor](../-migration-intent-processor/index.md). |
