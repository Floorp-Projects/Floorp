[android-components](../../../index.md) / [mozilla.components.concept.engine.media](../../index.md) / [Media](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L37)

Interface to be implemented by classes that want to observe a media element.

### Functions

| Name | Summary |
|---|---|
| [onMetadataChanged](on-metadata-changed.md) | `open fun onMetadataChanged(media: `[`Media`](../index.md)`, metadata: `[`Metadata`](../-metadata/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onPlaybackStateChanged](on-playback-state-changed.md) | `open fun onPlaybackStateChanged(media: `[`Media`](../index.md)`, playbackState: `[`PlaybackState`](../-playback-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
