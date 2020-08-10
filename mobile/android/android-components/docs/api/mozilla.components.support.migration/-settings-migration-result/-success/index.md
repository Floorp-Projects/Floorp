[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [SettingsMigrationResult](../index.md) / [Success](./index.md)

# Success

`sealed class Success : `[`SettingsMigrationResult`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecSettingsMigrator.kt#L108)

Successful setting migration.

### Types

| Name | Summary |
|---|---|
| [NoFennecPrefs](-no-fennec-prefs/index.md) | `object NoFennecPrefs : `[`Success`](./index.md)<br>Fennec app SharedPreference file is not accessible. |
| [SettingsMigrated](-settings-migrated/index.md) | `data class SettingsMigrated : `[`Success`](./index.md)<br>Migration work completed successfully. |

### Inheritors

| Name | Summary |
|---|---|
| [NoFennecPrefs](-no-fennec-prefs/index.md) | `object NoFennecPrefs : `[`Success`](./index.md)<br>Fennec app SharedPreference file is not accessible. |
| [SettingsMigrated](-settings-migrated/index.md) | `data class SettingsMigrated : `[`Success`](./index.md)<br>Migration work completed successfully. |
