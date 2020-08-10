[android-components](../../../index.md) / [mozilla.components.feature.pwa.feature](../../index.md) / [SiteControlsBuilder](../index.md) / [Default](index.md) / [buildNotification](./build-notification.md)

# buildNotification

`open fun buildNotification(context: <ERROR CLASS>, builder: Builder, channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/SiteControlsBuilder.kt#L51)

Overrides [SiteControlsBuilder.buildNotification](../build-notification.md)

Create the notification to be displayed. Initial values are set in the provided [builder](../build-notification.md#mozilla.components.feature.pwa.feature.SiteControlsBuilder$buildNotification(, androidx.core.app.NotificationCompat.Builder, kotlin.String)/builder)
and additional actions can be added here. Actions should be represented as [PendingIntent](#)
that are filtered by [getFilter](../get-filter.md) and handled in [onReceiveBroadcast](../on-receive-broadcast.md).

