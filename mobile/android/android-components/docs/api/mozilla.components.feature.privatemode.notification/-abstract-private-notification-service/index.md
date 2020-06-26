[android-components](../../index.md) / [mozilla.components.feature.privatemode.notification](../index.md) / [AbstractPrivateNotificationService](./index.md)

# AbstractPrivateNotificationService

`abstract class AbstractPrivateNotificationService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/privatemode/src/main/java/mozilla/components/feature/privatemode/notification/AbstractPrivateNotificationService.kt#L44)

Manages notifications for private tabs.

Private tab notifications solve two problems:

1. They allow users to interact with the browser from outside of the app
    (example: by closing all private tabs).
2. The notification will keep the process alive, allowing the browser to
    keep private tabs in memory.

As long as a private tab is open this service will keep its notification alive.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractPrivateNotificationService()`<br>Manages notifications for private tabs. |

### Properties

| Name | Summary |
|---|---|
| [store](store.md) | `abstract val store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md) |

### Functions

| Name | Summary |
|---|---|
| [buildNotification](build-notification.md) | `abstract fun Builder.buildNotification(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Customizes the private browsing notification. |
| [erasePrivateTabs](erase-private-tabs.md) | `open fun erasePrivateTabs(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Erases all private tabs in reaction to the user tapping the notification. |
| [onBind](on-bind.md) | `fun onBind(intent: <ERROR CLASS>?): <ERROR CLASS>?` |
| [onCreate](on-create.md) | `fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Create the private browsing notification and add a listener to stop the service once all private tabs are closed. |
| [onDestroy](on-destroy.md) | `fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStartCommand](on-start-command.md) | `fun onStartCommand(intent: <ERROR CLASS>, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, startId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onTaskRemoved](on-task-removed.md) | `fun onTaskRemoved(rootIntent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTION_ERASE](-a-c-t-i-o-n_-e-r-a-s-e.md) | `const val ACTION_ERASE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [NOTIFICATION_CHANNEL](-n-o-t-i-f-i-c-a-t-i-o-n_-c-h-a-n-n-e-l.md) | `val NOTIFICATION_CHANNEL: `[`ChannelData`](../../mozilla.components.support.ktx.android.notification/-channel-data/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
