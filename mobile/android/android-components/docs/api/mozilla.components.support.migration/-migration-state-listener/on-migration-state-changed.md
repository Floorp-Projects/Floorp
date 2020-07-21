[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [MigrationStateListener](index.md) / [onMigrationStateChanged](./on-migration-state-changed.md)

# onMigrationStateChanged

`abstract fun onMigrationStateChanged(progress: `[`MigrationProgress`](../../mozilla.components.support.migration.state/-migration-progress/index.md)`, results: `[`MigrationResults`](../-migration-results.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/AbstractMigrationProgressActivity.kt#L54)

Invoked on a migration state change.

### Parameters

`progress` - The progress of the entire migration process.

`results` - The updated results from any progress made.