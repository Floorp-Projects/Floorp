[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigrator](./index.md)

# FennecMigrator

`class FennecMigrator` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L67)

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
| [migrateAsync](migrate-async.md) | `fun migrateAsync(): Deferred<`[`MigrationResults`](../-migration-results.md)`>`<br>Performs configured data migration. See [Builder](-builder/index.md) for how to configure a data migration. |
