[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [MigrationIntentProcessor](index.md) / [process](./process.md)

# process

`fun process(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/MigrationIntentProcessor.kt#L23)

Overrides [IntentProcessor.process](../../mozilla.components.feature.intent.processing/-intent-processor/process.md)

Processes all incoming intents if a migration is in progress.

If this is true, we should show an instance of AbstractMigrationProgressActivity.

