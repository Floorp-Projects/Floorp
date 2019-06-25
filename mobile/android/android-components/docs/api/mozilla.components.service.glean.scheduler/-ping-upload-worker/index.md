[android-components](../../index.md) / [mozilla.components.service.glean.scheduler](../index.md) / [PingUploadWorker](./index.md)

# PingUploadWorker

`class PingUploadWorker : Worker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/scheduler/PingUploadWorker.kt#L23)

This class is the worker class used by [WorkManager](#) to handle uploading the ping to the server.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PingUploadWorker(context: <ERROR CLASS>, params: WorkerParameters)`<br>This class is the worker class used by [WorkManager](#) to handle uploading the ping to the server. |

### Functions

| Name | Summary |
|---|---|
| [doWork](do-work.md) | `fun doWork(): Result`<br>This method is called on a background thread - you are required to **synchronously** do your work and return the [androidx.work.ListenableWorker.Result](#) from this method.  Once you return from this method, the Worker is considered to have finished what its doing and will be destroyed. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [buildConstraints](build-constraints.md) | `fun buildConstraints(): Constraints`<br>Build the constraints around which the worker can be run, such as whether network connectivity is required. |
