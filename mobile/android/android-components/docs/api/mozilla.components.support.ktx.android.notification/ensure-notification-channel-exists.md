[android-components](../index.md) / [mozilla.components.support.ktx.android.notification](index.md) / [ensureNotificationChannelExists](./ensure-notification-channel-exists.md)

# ensureNotificationChannelExists

`fun ensureNotificationChannelExists(context: <ERROR CLASS>, channelDate: `[`ChannelData`](-channel-data/index.md)`, onSetupChannel: <ERROR CLASS>.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, onCreateChannel: <ERROR CLASS>.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/notification/Notification.kt#L25)

Make sure a notification channel exists.

### Parameters

`context` - A [Context](#), used for creating the notification channel.

`onSetupChannel` - A lambda in the context of the NotificationChannel that gives you the
opportunity to apply any setup on the channel before gets created.

`onCreateChannel` - A lambda in the context of the NotificationManager that gives you the
opportunity to perform any operation on the [NotificationManager](#).

**Return**
Returns the channel id to be used.

