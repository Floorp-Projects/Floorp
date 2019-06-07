[android-components](../../index.md) / [mozilla.components.support.base.ids](../index.md) / [androidx.core.app.NotificationManagerCompat](index.md) / [notify](./notify.md)

# notify

`fun NotificationManagerCompat.notify(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, notification: `[`Notification`](https://developer.android.com/reference/android/app/Notification.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/ids/NotificationIds.kt#L55)

Post a notification to be shown in the status bar.

Uses a unique [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) tag instead of an [Int](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) id like [NotificationManagerCompat.notify](#).

