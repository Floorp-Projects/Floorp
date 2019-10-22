[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [VersionedMigration](./index.md)

# VersionedMigration

`data class VersionedMigration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L50)

Describes a [Migration](../-migration/index.md) at a specific version, enforcing in-range version specification.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `VersionedMigration(migration: `[`Migration`](../-migration/index.md)`, version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = migration.currentVersion)`<br>Describes a [Migration](../-migration/index.md) at a specific version, enforcing in-range version specification. |

### Properties

| Name | Summary |
|---|---|
| [migration](migration.md) | `val migration: `[`Migration`](../-migration/index.md)<br>A [Migration](../-migration/index.md) in question. |
| [version](version.md) | `val version: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Version of the [migration](migration.md), defaulting to the current version. |
