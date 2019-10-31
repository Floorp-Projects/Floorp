[android-components](../../index.md) / [mozilla.components.feature.downloads](../index.md) / [AbstractFetchDownloadService](./index.md)

# AbstractFetchDownloadService

`abstract class AbstractFetchDownloadService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/AbstractFetchDownloadService.kt#L55)

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
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStartCommand](on-start-command.md) | `open fun onStartCommand(intent: <ERROR CLASS>?, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTION_CANCEL](-a-c-t-i-o-n_-c-a-n-c-e-l.md) | `const val ACTION_CANCEL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ACTION_OPEN](-a-c-t-i-o-n_-o-p-e-n.md) | `const val ACTION_OPEN: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ACTION_PAUSE](-a-c-t-i-o-n_-p-a-u-s-e.md) | `const val ACTION_PAUSE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ACTION_RESUME](-a-c-t-i-o-n_-r-e-s-u-m-e.md) | `const val ACTION_RESUME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ACTION_TRY_AGAIN](-a-c-t-i-o-n_-t-r-y_-a-g-a-i-n.md) | `const val ACTION_TRY_AGAIN: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
