[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [CoroutineService](./index.md)

# CoroutineService

`abstract class CoroutineService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/CoroutineService.kt#L23)

Service that runs suspend functions in parallel.
When all jobs are completed, the service is stopped automatically.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CoroutineService(jobDispatcher: CoroutineDispatcher = Dispatchers.IO)`<br>Service that runs suspend functions in parallel. When all jobs are completed, the service is stopped automatically. |

### Functions

| Name | Summary |
|---|---|
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops all jobs when the service is destroyed. |
| [onStartCommand](on-start-command.md) | `fun onStartCommand(intent: <ERROR CLASS>?, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Starts a job using [onStartCommand](#) then stops the service once all jobs are complete. |

### Inheritors

| Name | Summary |
|---|---|
| [AbstractFetchDownloadService](../-abstract-fetch-download-service/index.md) | `abstract class AbstractFetchDownloadService : `[`CoroutineService`](./index.md)<br>Service that performs downloads through a fetch [Client](../../mozilla.components.concept.fetch/-client/index.md) rather than through the native Android download manager. |
