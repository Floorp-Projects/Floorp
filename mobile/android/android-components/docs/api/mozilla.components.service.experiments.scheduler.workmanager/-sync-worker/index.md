[android-components](../../index.md) / [mozilla.components.service.experiments.scheduler.workmanager](../index.md) / [SyncWorker](./index.md)

# SyncWorker

`abstract class SyncWorker : Worker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/scheduler/workmanager/SyncWorker.kt#L12)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncWorker(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, params: WorkerParameters)` |

### Properties

| Name | Summary |
|---|---|
| [experiments](experiments.md) | `abstract val experiments: `[`Experiments`](../../mozilla.components.service.experiments/-experiments/index.md)<br>Used to provide the instance of Experiments the app is using |

### Functions

| Name | Summary |
|---|---|
| [doWork](do-work.md) | `open fun doWork(): Result` |
