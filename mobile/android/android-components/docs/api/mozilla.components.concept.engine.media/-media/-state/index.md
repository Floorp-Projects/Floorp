[android-components](../../../index.md) / [mozilla.components.concept.engine.media](../../index.md) / [Media](../index.md) / [State](./index.md)

# State

`enum class State` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L82)

A simplified media state deprived from the [playbackState](../playback-state.md) events.

### Enum Values

| Name | Summary |
|---|---|
| [UNKNOWN](-u-n-k-n-o-w-n.md) | Unknown. No state has been received from the engine yet. |
| [PAUSED](-p-a-u-s-e-d.md) | This [Media](../index.md) is paused. |
| [PLAYING](-p-l-a-y-i-n-g.md) | This [Media](../index.md) is currently playing. |
| [STOPPED](-s-t-o-p-p-e-d.md) | Playback of this [Media](../index.md) has stopped (either completed or aborted). |
