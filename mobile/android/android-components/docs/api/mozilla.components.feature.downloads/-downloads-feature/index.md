[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](./index.md)

# DownloadsFeature

`class DownloadsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsFeature.kt#L52)

Feature implementation to provide download functionality for the selected
session. The feature will subscribe to the selected session and listen
for downloads.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadsFeature(applicationContext: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, useCases: `[`DownloadsUseCases`](../-downloads-use-cases/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)` = { }, onDownloadCompleted: `[`OnDownloadCompleted`](../../mozilla.components.feature.downloads.manager/-on-download-completed.md)` = noop, downloadManager: `[`DownloadManager`](../../mozilla.components.feature.downloads.manager/-download-manager/index.md)` = AndroidDownloadManager(applicationContext), customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, dialog: `[`DownloadDialogFragment`](../-download-dialog-fragment/index.md)` = SimpleDownloadDialogFragment.newInstance())`<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `var onDownloadCompleted: `[`OnDownloadCompleted`](../../mozilla.components.feature.downloads.manager/-on-download-completed.md)<br>a callback invoked when a download is completed. |
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `var onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>a callback invoked when permissions need to be requested before a download can be performed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions request was completed. It will then either trigger or clear the pending download. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing downloads on the selected session and sends them to the [DownloadManager](../../mozilla.components.feature.downloads.manager/-download-manager/index.md) to be processed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing downloads on the selected session. |
