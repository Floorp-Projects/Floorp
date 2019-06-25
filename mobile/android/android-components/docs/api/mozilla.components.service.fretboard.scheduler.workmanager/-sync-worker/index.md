[android-components](../../index.md) / [mozilla.components.service.fretboard.scheduler.workmanager](../index.md) / [SyncWorker](./index.md)

# SyncWorker

`abstract class SyncWorker : Worker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/scheduler/workmanager/SyncWorker.kt#L12)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncWorker(context: <ERROR CLASS>, params: WorkerParameters)` |

### Properties

| Name | Summary |
|---|---|
| [fretboard](fretboard.md) | `abstract val fretboard: `[`Fretboard`](../../mozilla.components.service.fretboard/-fretboard/index.md)<br>Used to provide the instance of Fretboard the app is using |

### Functions

| Name | Summary |
|---|---|
| [doWork](do-work.md) | `open fun doWork(): Result` |
