[android-components](../index.md) / [mozilla.components.support.migration.state](./index.md)

## Package mozilla.components.support.migration.state

### Types

| Name | Summary |
|---|---|
| [MigrationAction](-migration-action/index.md) | `sealed class MigrationAction : `[`Action`](../mozilla.components.lib.state/-action.md)<br>Actions supported by the [MigrationStore](-migration-store/index.md). |
| [MigrationProgress](-migration-progress/index.md) | `enum class MigrationProgress`<br>The progress of the migration. |
| [MigrationState](-migration-state/index.md) | `data class MigrationState : `[`State`](../mozilla.components.lib.state/-state.md)<br>Value type that represents the state of the migration. |
| [MigrationStore](-migration-store/index.md) | `class MigrationStore : `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`MigrationState`](-migration-state/index.md)`, `[`MigrationAction`](-migration-action/index.md)`>`<br>[Store](../mozilla.components.lib.state/-store/index.md) keeping track of the current [MigrationState](-migration-state/index.md). |
