[android-components](../../index.md) / [mozilla.components.concept.engine.media](../index.md) / [Media](./index.md)

# Media

`abstract class Media : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L14)

Value type that represents a media element that is present on the currently displayed page in a session.

### Types

| Name | Summary |
|---|---|
| [Controller](-controller/index.md) | `interface Controller`<br>Controller for controlling playback of a media element. |
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe a media element. |
| [PlaybackState](-playback-state/index.md) | `enum class PlaybackState` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Media(delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`<br>Value type that represents a media element that is present on the currently displayed page in a session. |

### Properties

| Name | Summary |
|---|---|
| [controller](controller.md) | `abstract val controller: `[`Controller`](-controller/index.md)<br>The [Controller](-controller/index.md) for controlling playback of this media element. |
| [playbackState](playback-state.md) | `var playbackState: `[`PlaybackState`](-playback-state/index.md)<br>The current [PlaybackState](-playback-state/index.md) of this media element. |
