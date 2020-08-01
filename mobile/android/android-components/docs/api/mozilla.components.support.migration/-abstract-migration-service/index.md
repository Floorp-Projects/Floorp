[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [AbstractMigrationService](./index.md)

# AbstractMigrationService

`abstract class AbstractMigrationService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/AbstractMigrationService.kt#L39)

Abstract implementation of a background service running a configured [FennecMigrator](../-fennec-migrator/index.md).

An application using this implementation needs to extend this class and provide a
[FennecMigrator](../-fennec-migrator/index.md) instance.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractMigrationService()`<br>Abstract implementation of a background service running a configured [FennecMigrator](../-fennec-migrator/index.md). |

### Properties

| Name | Summary |
|---|---|
| [migrationDecisionActivity](migration-decision-activity.md) | `abstract val migrationDecisionActivity: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<out <ERROR CLASS>>` |
| [migrator](migrator.md) | `abstract val migrator: `[`FennecMigrator`](../-fennec-migrator/index.md) |
| [store](store.md) | `abstract val store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onBind](on-bind.md) | `open fun onBind(intent: <ERROR CLASS>?): <ERROR CLASS>?` |
| [onCreate](on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [dismissNotification](dismiss-notification.md) | `fun dismissNotification(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Dismisses the "migration completed" notification. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
