[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](./index.md)

# Builder

`class Builder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L80)

Data migration builder. Allows configuring which migrations to run, their versions and relative order.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Builder(context: <ERROR CLASS>)`<br>Data migration builder. Allows configuring which migrations to run, their versions and relative order. |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `fun build(): `[`FennecMigrator`](../index.md)<br>Constructs a [FennecMigrator](../index.md) based on the current configuration. |
| [migrateBookmarks](migrate-bookmarks.md) | `fun migrateBookmarks(storage: `[`PlacesBookmarksStorage`](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Bookmarks.currentVersion): `[`Builder`](./index.md)<br>Enable bookmarks migration. Must be called after [migrateHistory](migrate-history.md). |
| [migrateHistory](migrate-history.md) | `fun migrateHistory(storage: `[`PlacesHistoryStorage`](../../../mozilla.components.browser.storage.sync/-places-history-storage/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.History.currentVersion): `[`Builder`](./index.md)<br>Enable history migration. |
| [migrateOpenTabs](migrate-open-tabs.md) | `fun migrateOpenTabs(sessionManager: `[`SessionManager`](../../../mozilla.components.browser.session/-session-manager/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.OpenTabs.currentVersion): `[`Builder`](./index.md)<br>Enable open tabs migration. |
