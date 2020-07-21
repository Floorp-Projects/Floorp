[android-components](../../../index.md) / [mozilla.components.feature.addons.update](../../index.md) / [AddonUpdater](../index.md) / [Status](./index.md)

# Status

`sealed class Status` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L109)

Indicates the status of a request for updating an addon.

### Types

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`Status`](./index.md)<br>An error has happened while trying to update. |
| [NoUpdateAvailable](-no-update-available.md) | `object NoUpdateAvailable : `[`Status`](./index.md)<br>The addon has not been updated since the last update. |
| [NotInstalled](-not-installed.md) | `object NotInstalled : `[`Status`](./index.md)<br>The addon is not part of the installed list. |
| [SuccessfullyUpdated](-successfully-updated.md) | `object SuccessfullyUpdated : `[`Status`](./index.md)<br>The addon was successfully updated. |

### Extension Functions

| Name | Summary |
|---|---|
| [toLocalizedString](../../../mozilla.components.feature.addons.ui/to-localized-string.md) | `fun `[`Status`](./index.md)`?.toLocalizedString(context: <ERROR CLASS>): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Get the localized string for an [AddonUpdater.UpdateAttempt.status](../-update-attempt/status.md). |

### Inheritors

| Name | Summary |
|---|---|
| [Error](-error/index.md) | `data class Error : `[`Status`](./index.md)<br>An error has happened while trying to update. |
| [NoUpdateAvailable](-no-update-available.md) | `object NoUpdateAvailable : `[`Status`](./index.md)<br>The addon has not been updated since the last update. |
| [NotInstalled](-not-installed.md) | `object NotInstalled : `[`Status`](./index.md)<br>The addon is not part of the installed list. |
| [SuccessfullyUpdated](-successfully-updated.md) | `object SuccessfullyUpdated : `[`Status`](./index.md)<br>The addon was successfully updated. |
