[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [AbstractMigrationProgressActivity](./index.md)

# AbstractMigrationProgressActivity

`abstract class AbstractMigrationProgressActivity : AppCompatActivity, `[`MigrationStateListener`](../-migration-state-listener/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/AbstractMigrationProgressActivity.kt#L21)

An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor](../-migration-intent-processor/index.md).

Implementation notes: this abstraction exists to ensure that migration apps do not allow the user to
invoke [AppCompatActivity.onBackPressed](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractMigrationProgressActivity()`<br>An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor](../-migration-intent-processor/index.md). |

### Properties

| Name | Summary |
|---|---|
| [store](store.md) | `abstract val store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStart](on-start.md) | `open fun onStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStop](on-stop.md) | `open fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onMigrationCompleted](../-migration-state-listener/on-migration-completed.md) | `abstract fun onMigrationCompleted(results: `[`MigrationResults`](../-migration-results.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when the migration is complete. |
| [onMigrationStateChanged](../-migration-state-listener/on-migration-state-changed.md) | `abstract fun onMigrationStateChanged(progress: `[`MigrationProgress`](../../mozilla.components.support.migration.state/-migration-progress/index.md)`, results: `[`MigrationResults`](../-migration-results.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked on a migration state change. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
