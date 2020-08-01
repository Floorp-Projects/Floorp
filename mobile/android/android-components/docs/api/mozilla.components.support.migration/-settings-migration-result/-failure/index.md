[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [SettingsMigrationResult](../index.md) / [Failure](./index.md)

# Failure

`sealed class Failure : `[`SettingsMigrationResult`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecSettingsMigrator.kt#L134)

Failed settings migrations.

### Types

| Name | Summary |
|---|---|
| [MissingFHRPrefValue](-missing-f-h-r-pref-value/index.md) | `object MissingFHRPrefValue : `[`Failure`](./index.md)<br>Couldn't find FHR pref value in non-empty Fennec prefs. |
| [WrongTelemetryValueAfterMigration](-wrong-telemetry-value-after-migration/index.md) | `data class WrongTelemetryValueAfterMigration : `[`Failure`](./index.md)<br>Wrong telemetry value in Fenix after migration. |

### Inheritors

| Name | Summary |
|---|---|
| [MissingFHRPrefValue](-missing-f-h-r-pref-value/index.md) | `object MissingFHRPrefValue : `[`Failure`](./index.md)<br>Couldn't find FHR pref value in non-empty Fennec prefs. |
| [WrongTelemetryValueAfterMigration](-wrong-telemetry-value-after-migration/index.md) | `data class WrongTelemetryValueAfterMigration : `[`Failure`](./index.md)<br>Wrong telemetry value in Fenix after migration. |
