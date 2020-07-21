[android-components](../../../index.md) / [mozilla.components.feature.pwa.feature](../../index.md) / [SiteControlsBuilder](../index.md) / [CopyAndRefresh](./index.md)

# CopyAndRefresh

`class CopyAndRefresh : `[`Default`](../-default/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/SiteControlsBuilder.kt#L88)

Implementation of [SiteControlsBuilder](../index.md) that adds a Refresh button and
copies the URL of the site when tapped.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CopyAndRefresh(reloadUrlUseCase: `[`ReloadUrlUseCase`](../../../mozilla.components.feature.session/-session-use-cases/-reload-url-use-case/index.md)`)`<br>Implementation of [SiteControlsBuilder](../index.md) that adds a Refresh button and copies the URL of the site when tapped. |

### Functions

| Name | Summary |
|---|---|
| [buildNotification](build-notification.md) | `fun buildNotification(context: <ERROR CLASS>, builder: Builder, channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Create the notification to be displayed. Initial values are set in the provided [builder](../build-notification.md#mozilla.components.feature.pwa.feature.SiteControlsBuilder$buildNotification(, androidx.core.app.NotificationCompat.Builder, kotlin.String)/builder) and additional actions can be added here. Actions should be represented as [PendingIntent](#) that are filtered by [getFilter](../get-filter.md) and handled in [onReceiveBroadcast](../on-receive-broadcast.md). |
| [getFilter](get-filter.md) | `fun getFilter(): <ERROR CLASS>`<br>Return an intent filter that matches the actions specified in [buildNotification](../build-notification.md). |
| [onReceiveBroadcast](on-receive-broadcast.md) | `fun onReceiveBroadcast(context: <ERROR CLASS>, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handle actions the user selected in the site controls notification. |

### Inherited Functions

| Name | Summary |
|---|---|
| [createPendingIntent](../-default/create-pending-intent.md) | `fun createPendingIntent(context: <ERROR CLASS>, action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): <ERROR CLASS>` |
