[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [MigrationIntentProcessor](./index.md)

# MigrationIntentProcessor

`class MigrationIntentProcessor : `[`IntentProcessor`](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/MigrationIntentProcessor.kt#L17)

An [IntentProcessor](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) that checks if we're in a migration state.

⚠️ When using this processor, ensure this is the first processor to be invoked if there are multiple.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MigrationIntentProcessor(store: `[`MigrationStore`](../../mozilla.components.support.migration.state/-migration-store/index.md)`)`<br>An [IntentProcessor](../../mozilla.components.feature.intent.processing/-intent-processor/index.md) that checks if we're in a migration state. |

### Functions

| Name | Summary |
|---|---|
| [process](process.md) | `fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes all incoming intents if a migration is in progress. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
