[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [FennecMigrator](../index.md) / [Builder](index.md) / [migrateFxa](./migrate-fxa.md)

# migrateFxa

`fun migrateFxa(accountManager: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`FxaAccountManager`](../../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`>, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = Migration.FxA.currentVersion): `[`Builder`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L372)

Enable FxA state migration.

### Parameters

`accountManager` - An instance of [FxaAccountManager](../../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md) used for authenticating using a migrated account.

`version` - Version of the migration; defaults to the current version.