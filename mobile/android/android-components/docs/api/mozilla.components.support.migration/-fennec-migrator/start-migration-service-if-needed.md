[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigrator](index.md) / [startMigrationServiceIfNeeded](./start-migration-service-if-needed.md)

# startMigrationServiceIfNeeded

`fun <T : `[`AbstractMigrationService`](../-abstract-migration-service/index.md)`> startMigrationServiceIfNeeded(service: `[`Class`](https://developer.android.com/reference/java/lang/Class.html)`<`[`T`](start-migration-service-if-needed.md#T)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L297)

Starts the provided [AbstractMigrationService](../-abstract-migration-service/index.md) implementation if there are migrations to be
run for this installation.

