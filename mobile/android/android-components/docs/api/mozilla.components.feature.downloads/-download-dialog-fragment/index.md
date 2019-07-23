[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadDialogFragment](./index.md)

# DownloadDialogFragment

`abstract class DownloadDialogFragment : DialogFragment` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadDialogFragment.kt#L18)

This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature](../-downloads-feature/index.md)
to show a dialog before a download is triggered.
If [SimpleDownloadDialogFragment](../-simple-download-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class.
Be mindful to call [onStartDownload](on-start-download.md) when you want to start the download.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadDialogFragment()`<br>This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature](../-downloads-feature/index.md) to show a dialog before a download is triggered. If [SimpleDownloadDialogFragment](../-simple-download-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class. Be mindful to call [onStartDownload](on-start-download.md) when you want to start the download. |

### Properties

| Name | Summary |
|---|---|
| [onStartDownload](on-start-download.md) | `var onStartDownload: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A callback to trigger a download, call it when you are ready to start a download. For instance, a valid use case can be in confirmation dialog, after the positive button is clicked, this callback must be called. |

### Functions

| Name | Summary |
|---|---|
| [setDownload](set-download.md) | `fun setDownload(download: `[`Download`](../../mozilla.components.browser.session/-download/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>add the metadata of this download object to the arguments of this fragment. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [FRAGMENT_TAG](-f-r-a-g-m-e-n-t_-t-a-g.md) | `const val FRAGMENT_TAG: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [KEY_CONTENT_LENGTH](-k-e-y_-c-o-n-t-e-n-t_-l-e-n-g-t-h.md) | `const val KEY_CONTENT_LENGTH: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Key for finding the content length in the arguments. |
| [KEY_FILE_NAME](-k-e-y_-f-i-l-e_-n-a-m-e.md) | `const val KEY_FILE_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Key for finding the file name in the arguments. |
| [KEY_URL](-k-e-y_-u-r-l.md) | `const val KEY_URL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Key for finding the url in the arguments. |

### Inheritors

| Name | Summary |
|---|---|
| [SimpleDownloadDialogFragment](../-simple-download-dialog-fragment/index.md) | `class SimpleDownloadDialogFragment : `[`DownloadDialogFragment`](./index.md)<br>A confirmation dialog to be called before a download is triggered. Meant to be used in collaboration with [DownloadsFeature](../-downloads-feature/index.md) |
