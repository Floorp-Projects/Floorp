[android-components](../../../index.md) / [mozilla.components.support.migration](../../index.md) / [GeckoMigrationResult](../index.md) / [Success](./index.md)

# Success

`sealed class Success : `[`GeckoMigrationResult`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/GeckoMigration.kt#L34)

Success variants of a Gecko migration.

### Types

| Name | Summary |
|---|---|
| [NoPrefsFile](-no-prefs-file.md) | `object NoPrefsFile : `[`Success`](./index.md)<br>No prefs.js file present. |
| [PrefsFileMigrated](-prefs-file-migrated.md) | `object PrefsFileMigrated : `[`Success`](./index.md)<br>prefs.js file was successfully migrated with prefs we wanted to keep. |
| [PrefsFileRemovedInvalidPrefs](-prefs-file-removed-invalid-prefs.md) | `object PrefsFileRemovedInvalidPrefs : `[`Success`](./index.md)<br>prefs.js file was removed as we failed to transform prefs. |
| [PrefsFileRemovedNoPrefs](-prefs-file-removed-no-prefs.md) | `object PrefsFileRemovedNoPrefs : `[`Success`](./index.md)<br>prefs.js file was removed as it contained no prefs we wanted to keep. |

### Inheritors

| Name | Summary |
|---|---|
| [NoPrefsFile](-no-prefs-file.md) | `object NoPrefsFile : `[`Success`](./index.md)<br>No prefs.js file present. |
| [PrefsFileMigrated](-prefs-file-migrated.md) | `object PrefsFileMigrated : `[`Success`](./index.md)<br>prefs.js file was successfully migrated with prefs we wanted to keep. |
| [PrefsFileRemovedInvalidPrefs](-prefs-file-removed-invalid-prefs.md) | `object PrefsFileRemovedInvalidPrefs : `[`Success`](./index.md)<br>prefs.js file was removed as we failed to transform prefs. |
| [PrefsFileRemovedNoPrefs](-prefs-file-removed-no-prefs.md) | `object PrefsFileRemovedNoPrefs : `[`Success`](./index.md)<br>prefs.js file was removed as it contained no prefs we wanted to keep. |
