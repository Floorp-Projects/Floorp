[android-components](../../index.md) / [mozilla.components.feature.media.service](../index.md) / [AbstractMediaService](./index.md)

# AbstractMediaService

`abstract class AbstractMediaService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/service/AbstractMediaService.kt#L18)

A foreground service that will keep the process alive while we are playing media (with the app possibly in the
background) and shows an ongoing notification

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractMediaService()`<br>A foreground service that will keep the process alive while we are playing media (with the app possibly in the background) and shows an ongoing notification |

### Properties

| Name | Summary |
|---|---|
| [store](store.md) | `abstract val store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onBind](on-bind.md) | `open fun onBind(intent: <ERROR CLASS>?): <ERROR CLASS>?` |
| [onCreate](on-create.md) | `open fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStartCommand](on-start-command.md) | `open fun onStartCommand(intent: <ERROR CLASS>?, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onTaskRemoved](on-task-removed.md) | `open fun onTaskRemoved(rootIntent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTION_SWITCH_TAB](-a-c-t-i-o-n_-s-w-i-t-c-h_-t-a-b.md) | `const val ACTION_SWITCH_TAB: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [EXTRA_TAB_ID](-e-x-t-r-a_-t-a-b_-i-d.md) | `const val EXTRA_TAB_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [NOTIFICATION_TAG](-n-o-t-i-f-i-c-a-t-i-o-n_-t-a-g.md) | `const val NOTIFICATION_TAG: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [PENDING_INTENT_TAG](-p-e-n-d-i-n-g_-i-n-t-e-n-t_-t-a-g.md) | `const val PENDING_INTENT_TAG: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
