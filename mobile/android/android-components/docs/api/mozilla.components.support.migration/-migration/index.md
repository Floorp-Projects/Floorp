[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [Migration](./index.md)

# Migration

`sealed class Migration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L27)

Supported Fennec migrations and their current versions.

### Types

| Name | Summary |
|---|---|
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`Migration`](./index.md)<br>Migrates bookmarks. Must run after history was migrated. |
| [History](-history.md) | `object History : `[`Migration`](./index.md)<br>Migrates history (both "places" and "visits"). |
| [OpenTabs](-open-tabs.md) | `object OpenTabs : `[`Migration`](./index.md)<br>Migrates open tabs. |

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
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`Migration`](./index.md)<br>Migrates bookmarks. Must run after history was migrated. |
| [History](-history.md) | `object History : `[`Migration`](./index.md)<br>Migrates history (both "places" and "visits"). |
| [OpenTabs](-open-tabs.md) | `object OpenTabs : `[`Migration`](./index.md)<br>Migrates open tabs. |
