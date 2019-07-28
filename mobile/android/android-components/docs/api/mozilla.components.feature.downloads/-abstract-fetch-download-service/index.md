[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [AbstractFetchDownloadService](./index.md)

# AbstractFetchDownloadService

`abstract class AbstractFetchDownloadService : `[`CoroutineService`](../-coroutine-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/AbstractFetchDownloadService.kt#L49)

Service that performs downloads through a fetch [Client](../../mozilla.components.concept.fetch/-client/index.md) rather than through the native
Android download manager.

To use this service, you must create a subclass in your application and it to the manifest.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractFetchDownloadService()`<br>Service that performs downloads through a fetch [Client](../../mozilla.components.concept.fetch/-client/index.md) rather than through the native Android download manager. |

### Properties

| Name | Summary |
|---|---|
| [httpClient](http-client.md) | `abstract val httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onBind](on-bind.md) | `open fun onBind(intent: <ERROR CLASS>?): <ERROR CLASS>?` |
| [onCreate](on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onDestroy](../-coroutine-service/on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops all jobs when the service is destroyed. |
| [onStartCommand](../-coroutine-service/on-start-command.md) | `fun onStartCommand(intent: <ERROR CLASS>?, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Starts a job using [onStartCommand](#) then stops the service once all jobs are complete. |
