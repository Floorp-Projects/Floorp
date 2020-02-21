[android-components](../../../index.md) / [mozilla.components.feature.pwa.feature](../../index.md) / [SiteControlsBuilder](../index.md) / [Default](./index.md)

# Default

`open class Default : `[`SiteControlsBuilder`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/SiteControlsBuilder.kt#L45)

Default implementation of [SiteControlsBuilder](../index.md) that copies the URL of the site when tapped.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Default()`<br>Default implementation of [SiteControlsBuilder](../index.md) that copies the URL of the site when tapped. |

### Functions

| Name | Summary |
|---|---|
| [buildNotification](build-notification.md) | `open fun buildNotification(context: <ERROR CLASS>, builder: Builder, channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Create the notification to be displayed. Initial values are set in the provided [builder](../build-notification.md#mozilla.components.feature.pwa.feature.SiteControlsBuilder$buildNotification(, androidx.core.app.NotificationCompat.Builder, kotlin.String)/builder) and additional actions can be added here. Actions should be represented as [PendingIntent](#) that are filtered by [getFilter](../get-filter.md) and handled in [onReceiveBroadcast](../on-receive-broadcast.md). |
| [createPendingIntent](create-pending-intent.md) | `fun createPendingIntent(context: <ERROR CLASS>, action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): <ERROR CLASS>` |
| [getFilter](get-filter.md) | `open fun getFilter(): <ERROR CLASS>`<br>Return an intent filter that matches the actions specified in [buildNotification](../build-notification.md). |
| [onReceiveBroadcast](on-receive-broadcast.md) | `open fun onReceiveBroadcast(context: <ERROR CLASS>, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handle actions the user selected in the site controls notification. |

### Inheritors

| Name | Summary |
|---|---|
| [CopyAndRefresh](../-copy-and-refresh/index.md) | `class CopyAndRefresh : `[`Default`](./index.md)<br>Implementation of [SiteControlsBuilder](../index.md) that adds a Refresh button and copies the URL of the site when tapped. |
