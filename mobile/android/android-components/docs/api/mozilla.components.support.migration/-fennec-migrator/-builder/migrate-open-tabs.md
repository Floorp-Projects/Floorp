[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateOpenTabs](./migrate-open-tabs.md)

# migrateOpenTabs

`fun migrateOpenTabs(sessionManager: `[`SessionManager`](../../../mozilla.components.browser.session/-session-manager/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.OpenTabs.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L356)

Enable open tabs migration.

### Parameters

`sessionManager` - An instance of [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md) used for restoring migrated [SessionManager.Snapshot](../../../mozilla.components.browser.session/-session-manager/-snapshot/index.md).

`version` - Version of the migration; defaults to the current version.