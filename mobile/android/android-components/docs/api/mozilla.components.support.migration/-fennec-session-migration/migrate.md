[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecSessionMigration](index.md) / [migrate](./migrate.md)

# migrate

`fun migrate(profilePath: `[`File`](https://developer.android.com/reference/java/io/File.html)`): `[`MigrationResult`](../-migration-result/index.md)`<`[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecSessionMigration.kt#L27)

Ties to migrate the "open tabs" from the given [profilePath](migrate.md#mozilla.components.support.migration.FennecSessionMigration$migrate(java.io.File)/profilePath) and on success returns a
[SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) (wrapped in [MigrationResult.Success](../-migration-result/-success/index.md)).

