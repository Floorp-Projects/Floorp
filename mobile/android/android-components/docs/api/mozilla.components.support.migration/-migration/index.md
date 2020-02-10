[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [Migration](./index.md)

# Migration

`sealed class Migration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L60)

Supported Fennec migrations and their current versions.

### Types

| Name | Summary |
|---|---|
| [Addons](-addons.md) | `object Addons : `[`Migration`](./index.md)<br>Migrates / Disables all currently unsupported Add-ons. |
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`Migration`](./index.md)<br>Migrates bookmarks. Must run after history was migrated. |
| [FxA](-fx-a.md) | `object FxA : `[`Migration`](./index.md)<br>Migrates FxA state. |
| [Gecko](-gecko.md) | `object Gecko : `[`Migration`](./index.md)<br>Migrates Gecko(View) internal files. |
| [History](-history.md) | `object History : `[`Migration`](./index.md)<br>Migrates history (both "places" and "visits"). |
| [Logins](-logins.md) | `object Logins : `[`Migration`](./index.md)<br>Migrates logins. |
| [OpenTabs](-open-tabs.md) | `object OpenTabs : `[`Migration`](./index.md)<br>Migrates open tabs. |
| [PinnedSites](-pinned-sites.md) | `object PinnedSites : `[`Migration`](./index.md)<br>Migrates Fennec's pinned sites. |
| [SearchEngine](-search-engine.md) | `object SearchEngine : `[`Migration`](./index.md)<br>Migrates Fennec's default search engine. |
| [Settings](-settings.md) | `object Settings : `[`Migration`](./index.md)<br>Migrates all Fennec settings backed by SharedPreferences. |
| [TelemetryIdentifiers](-telemetry-identifiers.md) | `object TelemetryIdentifiers : `[`Migration`](./index.md)<br>Migrates Fennec's telemetry identifiers. |

### Properties

| Name | Summary |
|---|---|
| [currentVersion](current-version.md) | `val currentVersion: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Addons](-addons.md) | `object Addons : `[`Migration`](./index.md)<br>Migrates / Disables all currently unsupported Add-ons. |
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`Migration`](./index.md)<br>Migrates bookmarks. Must run after history was migrated. |
| [FxA](-fx-a.md) | `object FxA : `[`Migration`](./index.md)<br>Migrates FxA state. |
| [Gecko](-gecko.md) | `object Gecko : `[`Migration`](./index.md)<br>Migrates Gecko(View) internal files. |
| [History](-history.md) | `object History : `[`Migration`](./index.md)<br>Migrates history (both "places" and "visits"). |
| [Logins](-logins.md) | `object Logins : `[`Migration`](./index.md)<br>Migrates logins. |
| [OpenTabs](-open-tabs.md) | `object OpenTabs : `[`Migration`](./index.md)<br>Migrates open tabs. |
| [PinnedSites](-pinned-sites.md) | `object PinnedSites : `[`Migration`](./index.md)<br>Migrates Fennec's pinned sites. |
| [SearchEngine](-search-engine.md) | `object SearchEngine : `[`Migration`](./index.md)<br>Migrates Fennec's default search engine. |
| [Settings](-settings.md) | `object Settings : `[`Migration`](./index.md)<br>Migrates all Fennec settings backed by SharedPreferences. |
| [TelemetryIdentifiers](-telemetry-identifiers.md) | `object TelemetryIdentifiers : `[`Migration`](./index.md)<br>Migrates Fennec's telemetry identifiers. |
