[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](./index.md)

# DownloadsFeature

`class DownloadsFeature : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/DownloadsFeature.kt#L46)

Feature implementation to provide download functionality for the selected
session. The feature will subscribe to the selected session and listen
for downloads.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadsFeature(applicationContext: <ERROR CLASS>, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)` = { }, onDownloadCompleted: `[`OnDownloadCompleted`](../../mozilla.components.feature.downloads.manager/-on-download-completed.md)` = { _, _ -> }, downloadManager: `[`DownloadManager`](../../mozilla.components.feature.downloads.manager/-download-manager/index.md)` = AndroidDownloadManager(applicationContext, onDownloadCompleted), sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, dialog: `[`DownloadDialogFragment`](../-download-dialog-fragment/index.md)` = SimpleDownloadDialogFragment.newInstance())`<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |

### Properties

| Name | Summary |
|---|---|
| [onDownloadCompleted](on-download-completed.md) | `var onDownloadCompleted: `[`OnDownloadCompleted`](../../mozilla.components.feature.downloads.manager/-on-download-completed.md)<br>a callback invoked when a download is completed. |
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `var onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../-on-need-to-request-permissions.md)<br>a callback invoked when permissions need to be requested before a download can be performed. Once the request is completed, [onPermissionsResult](on-permissions-result.md) needs to be invoked. |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [onDownload](on-download.md) | `fun onDownload(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, download: `[`Download`](../../mozilla.components.browser.session/-download/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Notifies the [DownloadManager](../../mozilla.components.feature.downloads.manager/-download-manager/index.md) that a new download must be processed. |
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that the permissions request was completed. It will then either trigger or clear the pending download. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing downloads on the selected session and sends them to the [DownloadManager](../../mozilla.components.feature.downloads.manager/-download-manager/index.md) to be processed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing downloads on the selected session. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |
