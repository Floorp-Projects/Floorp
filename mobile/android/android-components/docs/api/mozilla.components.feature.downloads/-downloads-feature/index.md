[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](./index.md)

# DownloadsFeature

`class DownloadsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsFeature.kt#L54)

Feature implementation to provide download functionality for the selected
session. The feature will subscribe to the selected session and listen
for downloads.

### Types

| Name | Summary |
|---|---|
| [PromptsStyling](-prompts-styling/index.md) | `data class PromptsStyling`<br>Styling for the download dialog prompt |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadsFeature(applicationContext: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, useCases: `[`DownloadsUseCases`](../-downloads-use-cases/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)` = { }, onDownloadStopped: `[`onDownloadStopped`](../../mozilla.components.feature.downloads.manager/on-download-stopped.md)` = noop, downloadManager: `[`DownloadManager`](../../mozilla.components.feature.downloads.manager/-download-manager/index.md)` = AndroidDownloadManager(applicationContext, store), tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = null)`<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |

### Properties

| Name | Summary |
|---|---|
| [onDownloadStopped](on-download-stopped.md) | `var onDownloadStopped: `[`onDownloadStopped`](../../mozilla.components.feature.downloads.manager/on-download-stopped.md)<br>a callback invoked when a download is paused or completed. |
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `var onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>a callback invoked when permissions need to be requested before a download can be performed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions request was completed. It will then either trigger or clear the pending download. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing downloads on the selected session and sends them to the [DownloadManager](../../mozilla.components.feature.downloads.manager/-download-manager/index.md) to be processed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing downloads on the selected session. |
| [tryAgain](try-again.md) | `fun tryAgain(id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Calls the tryAgain function of the corresponding [DownloadManager](../../mozilla.components.feature.downloads.manager/-download-manager/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
