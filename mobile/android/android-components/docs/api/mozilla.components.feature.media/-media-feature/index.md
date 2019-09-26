[android-components](../../index.md) / [mozilla.components.feature.media](../index.md) / [MediaFeature](./index.md)

# MediaFeature

`class MediaFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/MediaFeature.kt#L25)

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

### Companion Object Properties

| Name | Summary |
|---|---|
| [ACTION_SWITCH_TAB](-a-c-t-i-o-n_-s-w-i-t-c-h_-t-a-b.md) | `const val ACTION_SWITCH_TAB: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [EXTRA_TAB_ID](-e-x-t-r-a_-t-a-b_-i-d.md) | `const val EXTRA_TAB_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
