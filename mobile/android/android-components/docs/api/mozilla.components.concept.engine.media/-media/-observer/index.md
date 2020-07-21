[android-components](../../../index.md) / [mozilla.components.concept.engine.media](../../index.md) / [Media](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L58)

Interface to be implemented by classes that want to observe a media element.

### Functions

| Name | Summary |
|---|---|
| [onFullscreenChanged](on-fullscreen-changed.md) | `open fun onFullscreenChanged(media: `[`Media`](../index.md)`, fullscreen: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the observer that media fullscreen changed. |
| [onMetadataChanged](on-metadata-changed.md) | `open fun onMetadataChanged(media: `[`Media`](../index.md)`, metadata: `[`Metadata`](../-metadata/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the observer that media metadata changed. |
| [onPlaybackStateChanged](on-playback-state-changed.md) | `open fun onPlaybackStateChanged(media: `[`Media`](../index.md)`, playbackState: `[`PlaybackState`](../-playback-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the observer that media playback state changed. |
| [onStateChanged](on-state-changed.md) | `open fun onStateChanged(media: `[`Media`](../index.md)`, state: `[`State`](../-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the observer that media state changed. |
| [onVolumeChanged](on-volume-changed.md) | `open fun onVolumeChanged(media: `[`Media`](../index.md)`, volume: `[`Volume`](../-volume/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify the observer that media volume changed. |
