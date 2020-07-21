[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigrator](./index.md)

# FennecMigrator

`class FennecMigrator` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L209)

Entrypoint for Fennec data migration. See [Builder](-builder/index.md) for public API.

### Parameters

`context` - Application context used for accessing the file system.

`migrations` - Describes ordering and versioning of migrations to run.

`historyStorage` - An optional instance of [PlacesHistoryStorage](../../mozilla.components.browser.storage.sync/-places-history-storage/index.md) used to store migrated history data.

`bookmarksStorage` - An optional instance of [PlacesBookmarksStorage](../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md) used to store migrated bookmarks data.

`coroutineContext` - An instance of [CoroutineContext](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html) used for executing async migration tasks.

### Types

| Name | Summary |
|---|---|
| [Builder](-builder/index.md) | `class Builder`<br>Data migration builder. Allows configuring which migrations to run, their versions and relative order. |

### Functions

| Name | Summary |
|---|---|
| [migrateAsync](migrate-async.md) | `fun migrateAsync(store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md)`): Deferred<`[`MigrationResults`](../-migration-results.md)`>`<br>Performs configured data migration. See [Builder](-builder/index.md) for how to configure a data migration. |
| [startMigrationIfNeeded](start-migration-if-needed.md) | `fun <T : `[`AbstractMigrationService`](../-abstract-migration-service/index.md)`> startMigrationIfNeeded(store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md)`, service: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<`[`T`](start-migration-if-needed.md#T)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If a migration is needed then invoking this method will update the [MigrationStore](../../mozilla.components.support.migration.state/-migration-store/index.md) and launch the provided [AbstractMigrationService](../-abstract-migration-service/index.md) implementation. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
