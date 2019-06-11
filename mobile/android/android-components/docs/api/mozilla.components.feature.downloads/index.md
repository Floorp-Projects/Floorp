[android-components](../index.md) / [mozilla.components.feature.downloads](./index.md)

## Package mozilla.components.feature.downloads

### Types

| Name | Summary |
|---|---|
| [DownloadDialogFragment](-download-dialog-fragment/index.md) | `abstract class DownloadDialogFragment : DialogFragment`<br>This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature](-downloads-feature/index.md) to show a dialog before a download is triggered. If [SimpleDownloadDialogFragment](-simple-download-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class. Be mindful to call [onStartDownload](-download-dialog-fragment/on-start-download.md) when you want to start the download. |
| [DownloadsFeature](-downloads-feature/index.md) | `class DownloadsFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |
| [SimpleDownloadDialogFragment](-simple-download-dialog-fragment/index.md) | `class SimpleDownloadDialogFragment : `[`DownloadDialogFragment`](-download-dialog-fragment/index.md)<br>A confirmation dialog to be called before a download is triggered. Meant to be used in collaboration with [DownloadsFeature](-downloads-feature/index.md) |

### Type Aliases

| Name | Summary |
|---|---|
| [OnNeedToRequestPermissions](-on-need-to-request-permissions.md) | `typealias OnNeedToRequestPermissions = (permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
