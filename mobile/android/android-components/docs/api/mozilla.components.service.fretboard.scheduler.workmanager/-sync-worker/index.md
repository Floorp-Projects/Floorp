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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
