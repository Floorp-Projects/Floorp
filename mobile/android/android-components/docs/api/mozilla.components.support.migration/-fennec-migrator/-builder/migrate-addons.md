[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateAddons](./migrate-addons.md)

# migrateAddons

`fun migrateAddons(engine: `[`Engine`](../../../mozilla.components.concept.engine/-engine/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.Settings.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L301)

Enables Add-on migration.

### Parameters

`engine` - an instance of [Engine](../../../mozilla.components.concept.engine/-engine/index.md) use to query installed add-ons.

`version` - Version of the migration; defaults to the current version.