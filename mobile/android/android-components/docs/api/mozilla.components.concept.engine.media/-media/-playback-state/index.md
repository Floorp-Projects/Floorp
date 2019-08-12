[android-components](../../../index.md) / [mozilla.components.concept.engine.media](../../index.md) / [Media](../index.md) / [PlaybackState](./index.md)

# PlaybackState

`enum class PlaybackState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L71)

### Enum Values

| Name | Summary |
|---|---|
| [UNKNOWN](-u-n-k-n-o-w-n.md) | Unknown. No state has been received from the engine yet. |
| [PLAY](-p-l-a-y.md) | The media is no longer paused, as a result of the play method, or the autoplay attribute. |
| [PLAYING](-p-l-a-y-i-n-g.md) | Sent when the media has enough data to start playing, after the play event, but also when recovering from being stalled, when looping media restarts, and after seeked, if it was playing before seeking. |
| [PAUSE](-p-a-u-s-e.md) | Sent when the playback state is changed to paused. |
| [ENDED](-e-n-d-e-d.md) | Sent when playback completes. |
| [SEEKING](-s-e-e-k-i-n-g.md) | Sent when a seek operation begins. |
| [SEEKED](-s-e-e-k-e-d.md) | Sent when a seek operation completes. |
| [STALLED](-s-t-a-l-l-e-d.md) | Sent when the user agent is trying to fetch media data, but data is unexpectedly not forthcoming. |
| [SUSPENDED](-s-u-s-p-e-n-d-e-d.md) | Sent when loading of the media is suspended. This may happen either because the download has completed or because it has been paused for any other reason. |
| [WAITING](-w-a-i-t-i-n-g.md) | Sent when the requested operation (such as playback) is delayed pending the completion of another operation (such as a seek). |
| [ABORT](-a-b-o-r-t.md) | Sent when playback is aborted; for example, if the media is playing and is restarted from the beginning, this event is sent. |
| [EMPTIED](-e-m-p-t-i-e-d.md) | The media has become empty. For example, this event is sent if the media has already been loaded, and the load() method is called to reload it. |
