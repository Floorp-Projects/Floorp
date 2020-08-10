[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [SiteControlsBuilder](./index.md)

# SiteControlsBuilder

`interface SiteControlsBuilder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/SiteControlsBuilder.kt#L23)

Callback for [WebAppSiteControlsFeature](../-web-app-site-controls-feature/index.md) that lets the displayed notification be customized.

### Types

| Name | Summary |
|---|---|
| [CopyAndRefresh](-copy-and-refresh/index.md) | `class CopyAndRefresh : `[`Default`](-default/index.md)<br>Implementation of [SiteControlsBuilder](./index.md) that adds a Refresh button and copies the URL of the site when tapped. |
| [Default](-default/index.md) | `open class Default : `[`SiteControlsBuilder`](./index.md)<br>Default implementation of [SiteControlsBuilder](./index.md) that copies the URL of the site when tapped. |

### Functions

| Name | Summary |
|---|---|
| [buildNotification](build-notification.md) | `abstract fun buildNotification(context: <ERROR CLASS>, builder: Builder, channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Create the notification to be displayed. Initial values are set in the provided [builder](build-notification.md#mozilla.components.feature.pwa.feature.SiteControlsBuilder$buildNotification(, androidx.core.app.NotificationCompat.Builder, kotlin.String)/builder) and additional actions can be added here. Actions should be represented as [PendingIntent](#) that are filtered by [getFilter](get-filter.md) and handled in [onReceiveBroadcast](on-receive-broadcast.md). |
| [getFilter](get-filter.md) | `abstract fun getFilter(): <ERROR CLASS>`<br>Return an intent filter that matches the actions specified in [buildNotification](build-notification.md). |
| [onReceiveBroadcast](on-receive-broadcast.md) | `abstract fun onReceiveBroadcast(context: <ERROR CLASS>, session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, intent: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handle actions the user selected in the site controls notification. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Default](-default/index.md) | `open class Default : `[`SiteControlsBuilder`](./index.md)<br>Default implementation of [SiteControlsBuilder](./index.md) that copies the URL of the site when tapped. |
