[android-components](../index.md) / [mozilla.components.feature.downloads](./index.md)

## Package mozilla.components.feature.downloads

### Types

| Name | Summary |
|---|---|
| [AbstractFetchDownloadService](-abstract-fetch-download-service/index.md) | `abstract class AbstractFetchDownloadService : `[`CoroutineService`](-coroutine-service/index.md)<br>Service that performs downloads through a fetch [Client](../mozilla.components.concept.fetch/-client/index.md) rather than through the native Android download manager. |
| [CoroutineService](-coroutine-service/index.md) | `abstract class CoroutineService`<br>Service that runs suspend functions in parallel. When all jobs are completed, the service is stopped automatically. |
| [DownloadDialogFragment](-download-dialog-fragment/index.md) | `abstract class DownloadDialogFragment : DialogFragment`<br>This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature](-downloads-feature/index.md) to show a dialog before a download is triggered. If [SimpleDownloadDialogFragment](-simple-download-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class. Be mindful to call [onStartDownload](-download-dialog-fragment/on-start-download.md) when you want to start the download. |
| [DownloadsFeature](-downloads-feature/index.md) | `class DownloadsFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../mozilla.components.support.base.feature/-permissions-feature/index.md)<br>Feature implementation to provide download functionality for the selected session. The feature will subscribe to the selected session and listen for downloads. |
| [DownloadsUseCases](-downloads-use-cases/index.md) | `class DownloadsUseCases`<br>Contains use cases related to the downloads feature. |
| [SimpleDownloadDialogFragment](-simple-download-dialog-fragment/index.md) | `class SimpleDownloadDialogFragment : `[`DownloadDialogFragment`](-download-dialog-fragment/index.md)<br>A confirmation dialog to be called before a download is triggered. Meant to be used in collaboration with [DownloadsFeature](-downloads-feature/index.md) |
