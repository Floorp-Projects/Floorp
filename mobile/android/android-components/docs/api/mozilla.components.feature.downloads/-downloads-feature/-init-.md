[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [DownloadsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`DownloadsFeature(applicationContext: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, useCases: `[`DownloadsUseCases`](../-downloads-use-cases/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)` = { }, onDownloadStopped: `[`onDownloadStopped`](../../mozilla.components.feature.downloads.manager/on-download-stopped.md)` = noop, downloadManager: `[`DownloadManager`](../../mozilla.components.feature.downloads.manager/-download-manager/index.md)` = AndroidDownloadManager(applicationContext, store), tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fragmentManager: FragmentManager? = null, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = null, dialog: `[`DownloadDialogFragment`](../-download-dialog-fragment/index.md)` = SimpleDownloadDialogFragment.newInstance(
        promptsStyling = promptsStyling
    ))`

Feature implementation to provide download functionality for the selected
session. The feature will subscribe to the selected session and listen
for downloads.

