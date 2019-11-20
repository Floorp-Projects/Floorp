[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigratorException](./index.md)

# FennecMigratorException

`sealed class FennecMigratorException : `[`Exception`](https://developer.android.com/reference/java/lang/Exception.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L76)

Exceptions related to Fennec migrations.

See https://github.com/mozilla-mobile/android-components/issues/5095 for stripping any possible PII from these
exceptions.

### Exceptions

| Name | Summary |
|---|---|
| [MigrateBookmarksException](-migrate-bookmarks-exception/index.md) | `class MigrateBookmarksException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating bookmarks. |
| [MigrateGeckoException](-migrate-gecko-exception/index.md) | `class MigrateGeckoException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating gecko profile. |
| [MigrateHistoryException](-migrate-history-exception/index.md) | `class MigrateHistoryException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating history. |
| [MigrateOpenTabsException](-migrate-open-tabs-exception/index.md) | `class MigrateOpenTabsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating open tabs. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MigrateBookmarksException](-migrate-bookmarks-exception/index.md) | `class MigrateBookmarksException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating bookmarks. |
| [MigrateGeckoException](-migrate-gecko-exception/index.md) | `class MigrateGeckoException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating gecko profile. |
| [MigrateHistoryException](-migrate-history-exception/index.md) | `class MigrateHistoryException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating history. |
| [MigrateOpenTabsException](-migrate-open-tabs-exception/index.md) | `class MigrateOpenTabsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating open tabs. |
