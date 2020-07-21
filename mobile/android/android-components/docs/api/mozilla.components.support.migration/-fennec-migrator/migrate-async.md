[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigrator](index.md) / [migrateAsync](./migrate-async.md)

# migrateAsync

`fun migrateAsync(store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md)`): Deferred<`[`MigrationResults`](../-migration-results.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L491)

Performs configured data migration. See [Builder](-builder/index.md) for how to configure a data migration.

**Return**
A deferred [MigrationResults](../-migration-results.md), describing which migrations were performed and if they succeeded.

