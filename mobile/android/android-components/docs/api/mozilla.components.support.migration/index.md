[android-components](../index.md) / [mozilla.components.support.migration](./index.md)

## Package mozilla.components.support.migration

### Types

| Name | Summary |
|---|---|
| [AbstractMigrationProgressActivity](-abstract-migration-progress-activity/index.md) | `abstract class AbstractMigrationProgressActivity : AppCompatActivity, `[`MigrationStateListener`](-migration-state-listener/index.md)<br>An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor](-migration-intent-processor/index.md). |
| [AbstractMigrationService](-abstract-migration-service/index.md) | `abstract class AbstractMigrationService`<br>Abstract implementation of a background service running a configured [FennecMigrator](-fennec-migrator/index.md). |
| [AddonMigrationResult](-addon-migration-result/index.md) | `sealed class AddonMigrationResult`<br>Result of an add-on migration. |
| [FennecMigrator](-fennec-migrator/index.md) | `class FennecMigrator`<br>Entrypoint for Fennec data migration. See [Builder](-fennec-migrator/-builder/index.md) for public API. |
| [FennecProfile](-fennec-profile/index.md) | `data class FennecProfile`<br>A profile of "Fennec" (Firefox for Android). |
| [FxaMigrationResult](-fxa-migration-result/index.md) | `sealed class FxaMigrationResult`<br>Result of an FxA migration. |
| [GeckoMigrationResult](-gecko-migration-result/index.md) | `sealed class GeckoMigrationResult`<br>Result of a Gecko migration. |
| [Migration](-migration/index.md) | `sealed class Migration`<br>Supported Fennec migrations and their current versions. |
| [MigrationFacts](-migration-facts/index.md) | `class MigrationFacts`<br>Facts emitted for telemetry related to migration. |
| [MigrationIntentProcessor](-migration-intent-processor/index.md) | `class MigrationIntentProcessor : `[`IntentProcessor`](../mozilla.components.feature.intent.processing/-intent-processor/index.md)<br>An [IntentProcessor](../mozilla.components.feature.intent.processing/-intent-processor/index.md) that checks if we're in a migration state. |
| [MigrationRun](-migration-run/index.md) | `data class MigrationRun`<br>Results of running a single versioned migration. |
| [MigrationStateListener](-migration-state-listener/index.md) | `interface MigrationStateListener`<br>Interface to be implemented by classes that want to observe the migration state changes. |
| [Result](-result/index.md) | `sealed class Result<T>`<br>Class representing the result of a successful or failed migration action. |
| [SettingsMigrationResult](-settings-migration-result/index.md) | `sealed class SettingsMigrationResult`<br>Result of Fennec settings migration. |
| [TelemetryIdentifiersResult](-telemetry-identifiers-result/index.md) | `sealed class TelemetryIdentifiersResult`<br>Result of a telemetry identifier migration. |
| [VersionedMigration](-versioned-migration/index.md) | `data class VersionedMigration`<br>Describes a [Migration](-migration/index.md) at a specific version, enforcing in-range version specification. |

### Exceptions

| Name | Summary |
|---|---|
| [AddonMigrationException](-addon-migration-exception/index.md) | `class AddonMigrationException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wraps [AddonMigrationResult](-addon-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](-result/-failure/index.md). |
| [FennecMigratorException](-fennec-migrator-exception/index.md) | `sealed class FennecMigratorException : `[`Exception`](http://docs.oracle.com/javase/7/docs/api/java/lang/Exception.html)<br>Exceptions related to Fennec migrations. |
| [FennecProfileException](-fennec-profile-exception/index.md) | `sealed class FennecProfileException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Exceptions related to Fennec profile migrations. |
| [FxaMigrationException](-fxa-migration-exception/index.md) | `class FxaMigrationException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wraps [FxaMigrationResult](-fxa-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](-result/-failure/index.md). |
| [GeckoMigrationException](-gecko-migration-exception/index.md) | `class GeckoMigrationException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wraps [GeckoMigrationResult](-gecko-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](-result/-failure/index.md). |
| [SettingsMigrationException](-settings-migration-exception/index.md) | `class SettingsMigrationException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wraps [SettingsMigrationResult](-settings-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](-result/-failure/index.md). |

### Type Aliases

| Name | Summary |
|---|---|
| [MigrationResults](-migration-results.md) | `typealias MigrationResults = `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`Migration`](-migration/index.md)`, `[`MigrationRun`](-migration-run/index.md)`>`<br>Results of running a set of migrations. |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [java.lang.Exception](java.lang.-exception/index.md) |  |
