[android-components](../index.md) / [mozilla.components.support.utils](index.md) / [asForegroundServicePendingIntent](./as-foreground-service-pending-intent.md)

# asForegroundServicePendingIntent

`@JvmName("createForegroundServicePendingIntent") fun <ERROR CLASS>.asForegroundServicePendingIntent(context: <ERROR CLASS>, requestCode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = PendingIntent.FLAG_UPDATE_CURRENT): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/intents.kt#L24)

Create a [PendingIntent](#) instance to run a certain service described with the [Intent](#).

This method will allow you to launch a service that will be able to overpass
[background service limitations](https://developer.android.com/about/versions/oreo/background#services)
introduced in Android Oreo.

### Parameters

`context` - an [Intent](#) to start a service.