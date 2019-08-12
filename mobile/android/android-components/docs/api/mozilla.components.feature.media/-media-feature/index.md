[android-components](../../index.md) / [mozilla.components.feature.media](../index.md) / [MediaFeature](./index.md)

# MediaFeature

`class MediaFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/MediaFeature.kt#L23)

Feature implementation for media playback in web content. This feature takes care of:

* Background playback without the app getting killed.
* Showing a media notification with play/pause controls.
* Audio Focus handling (pausing/resuming in agreement with other media apps).
* Support for hardware controls to toggle play/pause (e.g. buttons on a headset)

This feature should get initialized globally once on app start and requires a started
[MediaStateMachine](../../mozilla.components.feature.media.state/-media-state-machine/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MediaFeature(context: <ERROR CLASS>)`<br>Feature implementation for media playback in web content. This feature takes care of: |

### Functions

| Name | Summary |
|---|---|
| [enable](enable.md) | `fun enable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables the feature. |
