[android-components](../../index.md) / [mozilla.components.feature.media](../index.md) / [MediaFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`MediaFeature(context: <ERROR CLASS>)`

Feature implementation for media playback in web content. This feature takes care of:

* Background playback without the app getting killed.
* Showing a media notification with play/pause controls.
* Audio Focus handling (pausing/resuming in agreement with other media apps).
* Support for hardware controls to toggle play/pause (e.g. buttons on a headset)

This feature should get initialized globally once on app start and requires a started
[MediaStateMachine](../../mozilla.components.feature.media.state/-media-state-machine/index.md).

