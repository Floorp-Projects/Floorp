[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [VersionedMigration](./index.md)

# VersionedMigration

`data class VersionedMigration` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecMigrator.kt#L123)

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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
