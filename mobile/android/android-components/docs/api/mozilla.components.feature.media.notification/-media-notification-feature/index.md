[android-components](../../index.md) / [mozilla.components.feature.media.notification](../index.md) / [MediaNotificationFeature](./index.md)

# MediaNotificationFeature

`class MediaNotificationFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/notification/MediaNotificationFeature.kt#L19)

Feature for displaying an ongoing notification (keeping the app process alive) while web content is playing media.

This feature should get initialized globally once on app start.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MediaNotificationFeature(context: <ERROR CLASS>, stateMachine: `[`MediaStateMachine`](../../mozilla.components.feature.media.state/-media-state-machine/index.md)`)`<br>Feature for displaying an ongoing notification (keeping the app process alive) while web content is playing media. |

### Functions

| Name | Summary |
|---|---|
| [enable](enable.md) | `fun enable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables the feature. |
