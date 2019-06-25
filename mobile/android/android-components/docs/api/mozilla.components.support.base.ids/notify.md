[android-components](../index.md) / [mozilla.components.support.base.ids](index.md) / [notify](./notify.md)

# notify

`fun <ERROR CLASS>.notify(context: <ERROR CLASS>, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, notification: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/ids/NotificationIds.kt#L46)

Post a notification to be shown in the status bar.

Uses a unique [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) tag instead of an [Int](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) id like [NotificationManager.notify](#).

