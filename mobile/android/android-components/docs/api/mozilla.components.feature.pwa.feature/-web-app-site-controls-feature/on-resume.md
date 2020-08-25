[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppSiteControlsFeature](index.md) / [onResume](./on-resume.md)

# onResume

`fun onResume(owner: LifecycleOwner): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/WebAppSiteControlsFeature.kt#L90)

Displays a notification from the given [SiteControlsBuilder.buildNotification](../-site-controls-builder/build-notification.md) that will be
shown as long as the lifecycle is in the foreground. Registers this class as a broadcast
receiver to receive events from the notification and call [SiteControlsBuilder.onReceiveBroadcast](../-site-controls-builder/on-receive-broadcast.md).

