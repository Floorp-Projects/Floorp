[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecMigratorException](./index.md)

# FennecMigratorException

`sealed class FennecMigratorException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L137)

Exceptions related to Fennec migrations.

See https://github.com/mozilla-mobile/android-components/issues/5095 for stripping any possible PII from these
exceptions.

### Exceptions

| Name | Summary |
|---|---|
| [HighLevel](-high-level/index.md) | `class HighLevel : `[`FennecMigratorException`](./index.md)<br>Unexpected exception during high level migration processing. |
| [MigrateAddonsException](-migrate-addons-exception/index.md) | `class MigrateAddonsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating addons. |
| [MigrateBookmarksException](-migrate-bookmarks-exception/index.md) | `class MigrateBookmarksException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating bookmarks. |
| [MigrateFxaException](-migrate-fxa-exception/index.md) | `class MigrateFxaException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating FxA. |
| [MigrateGeckoException](-migrate-gecko-exception/index.md) | `class MigrateGeckoException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating gecko profile. |
| [MigrateHistoryException](-migrate-history-exception/index.md) | `class MigrateHistoryException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating history. |
| [MigrateLoginsException](-migrate-logins-exception/index.md) | `class MigrateLoginsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating logins. |
| [MigrateOpenTabsException](-migrate-open-tabs-exception/index.md) | `class MigrateOpenTabsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating open tabs. |
| [MigratePinnedSitesException](-migrate-pinned-sites-exception/index.md) | `class MigratePinnedSitesException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating pinned sites. |
| [MigrateSearchEngineException](-migrate-search-engine-exception/index.md) | `class MigrateSearchEngineException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating the default search engine. |
| [MigrateSettingsException](-migrate-settings-exception/index.md) | `class MigrateSettingsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating settings. |
| [TelemetryIdentifierException](-telemetry-identifier-exception/index.md) | `class TelemetryIdentifierException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating telemetry identifiers. |

### Extension Functions

| Name | Summary |
|---|---|
| [getStacktraceAsString](../../mozilla.components.support.base.ext/kotlin.-throwable/get-stacktrace-as-string.md) | `fun `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`.getStacktraceAsString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a formatted string of the [Throwable.stackTrace](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/stack-trace.html). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [uniqueId](../java.lang.-exception/unique-id.md) | `fun `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`.uniqueId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a unique identifier for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [HighLevel](-high-level/index.md) | `class HighLevel : `[`FennecMigratorException`](./index.md)<br>Unexpected exception during high level migration processing. |
| [MigrateAddonsException](-migrate-addons-exception/index.md) | `class MigrateAddonsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating addons. |
| [MigrateBookmarksException](-migrate-bookmarks-exception/index.md) | `class MigrateBookmarksException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating bookmarks. |
| [MigrateFxaException](-migrate-fxa-exception/index.md) | `class MigrateFxaException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating FxA. |
| [MigrateGeckoException](-migrate-gecko-exception/index.md) | `class MigrateGeckoException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating gecko profile. |
| [MigrateHistoryException](-migrate-history-exception/index.md) | `class MigrateHistoryException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating history. |
| [MigrateLoginsException](-migrate-logins-exception/index.md) | `class MigrateLoginsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating logins. |
| [MigrateOpenTabsException](-migrate-open-tabs-exception/index.md) | `class MigrateOpenTabsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating open tabs. |
| [MigratePinnedSitesException](-migrate-pinned-sites-exception/index.md) | `class MigratePinnedSitesException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating pinned sites. |
| [MigrateSearchEngineException](-migrate-search-engine-exception/index.md) | `class MigrateSearchEngineException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating the default search engine. |
| [MigrateSettingsException](-migrate-settings-exception/index.md) | `class MigrateSettingsException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating settings. |
| [TelemetryIdentifierException](-telemetry-identifier-exception/index.md) | `class TelemetryIdentifierException : `[`FennecMigratorException`](./index.md)<br>Unexpected exception while migrating telemetry identifiers. |
