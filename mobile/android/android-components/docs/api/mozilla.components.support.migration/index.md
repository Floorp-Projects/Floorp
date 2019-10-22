[android-components](../index.md) / [mozilla.components.support.migration](./index.md)

## Package mozilla.components.support.migration

### Types

| Name | Summary |
|---|---|
| [FennecMigrator](-fennec-migrator/index.md) | `class FennecMigrator`<br>Entrypoint for Fennec data migration. See [Builder](-fennec-migrator/-builder/index.md) for public API. |
| [FennecProfile](-fennec-profile/index.md) | `data class FennecProfile`<br>A profile of "Fennec" (Firefox for Android). |
| [FennecSessionMigration](-fennec-session-migration/index.md) | `object FennecSessionMigration`<br>Helper for importing/migrating "open tabs" from Fennec. |
| [Migration](-migration/index.md) | `sealed class Migration`<br>Supported Fennec migrations and their current versions. |
| [MigrationRun](-migration-run/index.md) | `data class MigrationRun`<br>Results of running a single versioned migration. |
| [Result](-result/index.md) | `sealed class Result<T>`<br>Class representing the result of a successful or failed migration action. |
| [VersionedMigration](-versioned-migration/index.md) | `data class VersionedMigration`<br>Describes a [Migration](-migration/index.md) at a specific version, enforcing in-range version specification. |

### Type Aliases

| Name | Summary |
|---|---|
| [MigrationResults](-migration-results.md) | `typealias MigrationResults = `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`Migration`](-migration/index.md)`, `[`MigrationRun`](-migration-run/index.md)`>`<br>Results of running a set of migrations. |
