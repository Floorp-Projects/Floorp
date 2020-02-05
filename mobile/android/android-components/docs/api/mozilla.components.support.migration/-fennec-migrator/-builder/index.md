[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](./index.md)

# Builder

`class Builder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L206)

Data migration builder. Allows configuring which migrations to run, their versions and relative order.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Builder(context: <ERROR CLASS>, crashReporter: `[`CrashReporter`](../../../mozilla.components.lib.crash/-crash-reporter/index.md)`)`<br>Data migration builder. Allows configuring which migrations to run, their versions and relative order. |

### Functions

| Name | Summary |
|---|---|
| [build](build.md) | `fun build(): `[`FennecMigrator`](../index.md)<br>Constructs a [FennecMigrator](../index.md) based on the current configuration. |
| [migrateAddons](migrate-addons.md) | `fun migrateAddons(engine: `[`Engine`](../../../mozilla.components.concept.engine/-engine/index.md)`, addonCollectionProvider: `[`AddonCollectionProvider`](../../../mozilla.components.feature.addons.amo/-addon-collection-provider/index.md)`, addonUpdater: `[`AddonUpdater`](../../../mozilla.components.feature.addons.update/-addon-updater/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Addons.currentVersion): `[`Builder`](./index.md)<br>Enables Add-on migration. |
| [migrateBookmarks](migrate-bookmarks.md) | `fun migrateBookmarks(storage: `[`PlacesBookmarksStorage`](../../../mozilla.components.browser.storage.sync/-places-bookmarks-storage/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Bookmarks.currentVersion): `[`Builder`](./index.md)<br>Enable bookmarks migration. Must be called after [migrateHistory](migrate-history.md). |
| [migrateFxa](migrate-fxa.md) | `fun migrateFxa(accountManager: `[`FxaAccountManager`](../../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.FxA.currentVersion): `[`Builder`](./index.md)<br>Enable FxA state migration. |
| [migrateGecko](migrate-gecko.md) | `fun migrateGecko(version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Gecko.currentVersion): `[`Builder`](./index.md)<br>Enables the migration of Gecko internal files. |
| [migrateHistory](migrate-history.md) | `fun migrateHistory(storage: `[`PlacesHistoryStorage`](../../../mozilla.components.browser.storage.sync/-places-history-storage/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.History.currentVersion): `[`Builder`](./index.md)<br>Enable history migration. |
| [migrateLogins](migrate-logins.md) | `fun migrateLogins(storage: `[`AsyncLoginsStorage`](../../../mozilla.components.service.sync.logins/-async-logins-storage/index.md)`, storageKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Logins.currentVersion): `[`Builder`](./index.md)<br>Enable logins migration. |
| [migrateOpenTabs](migrate-open-tabs.md) | `fun migrateOpenTabs(sessionManager: `[`SessionManager`](../../../mozilla.components.browser.session/-session-manager/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.OpenTabs.currentVersion): `[`Builder`](./index.md)<br>Enable open tabs migration. |
| [migrateSettings](migrate-settings.md) | `fun migrateSettings(version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Settings.currentVersion): `[`Builder`](./index.md)<br>Enable all Fennec - Fenix common settings migration. |
| [migrateTelemetryIdentifiers](migrate-telemetry-identifiers.md) | `fun migrateTelemetryIdentifiers(version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.TelemetryIdentifiers.currentVersion): `[`Builder`](./index.md)<br>Enable migration of Fennec telemetry identifiers. |
