[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigrator](index.md) / [startMigrationIfNeeded](./start-migration-if-needed.md)

# startMigrationIfNeeded

`fun <T : `[`AbstractMigrationService`](../-abstract-migration-service/index.md)`> startMigrationIfNeeded(store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md)`, service: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<`[`T`](start-migration-if-needed.md#T)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L548)

If a migration is needed then invoking this method will update the [MigrationStore](../../mozilla.components.support.migration.state/-migration-store/index.md) and launch
the provided [AbstractMigrationService](../-abstract-migration-service/index.md) implementation.

