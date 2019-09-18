[android-components](../../index.md) / [mozilla.components.feature.media.state](../index.md) / [MediaState](./index.md)

# MediaState

`sealed class MediaState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/state/MediaState.kt#L13)

Accumulated state of all [Media](../../mozilla.components.concept.engine.media/-media/index.md) of all [Session](../../mozilla.components.browser.session/-session/index.md)s.

### Types

| Name | Summary |
|---|---|
| [None](-none/index.md) | `object None : `[`MediaState`](./index.md)<br>None: No media state. |
| [Paused](-paused/index.md) | `data class Paused : `[`MediaState`](./index.md)<br>Paused: [media](-paused/media.md) of [session](-paused/session.md) is currently paused. |
| [Playing](-playing/index.md) | `data class Playing : `[`MediaState`](./index.md)<br>Playing: [media](-playing/media.md) of [session](-playing/session.md) is currently playing. |

### Extension Functions

| Name | Summary |
|---|---|
| [getSession](../../mozilla.components.feature.media.ext/get-session.md) | `fun `[`MediaState`](./index.md)`.getSession(): `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>Get the [Session](../../mozilla.components.browser.session/-session/index.md) that caused this [MediaState](./index.md) (if any). |
| [isForCustomTabSession](../../mozilla.components.feature.media.ext/is-for-custom-tab-session.md) | `fun `[`MediaState`](./index.md)`.isForCustomTabSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this [MediaState](./index.md) is associated with a Custom Tab [Session](../../mozilla.components.browser.session/-session/index.md). |
| [pauseIfPlaying](../../mozilla.components.feature.media.ext/pause-if-playing.md) | `fun `[`MediaState`](./index.md)`.pauseIfPlaying(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.Playing](-playing/index.md) then pause all playing [Media](../../mozilla.components.concept.engine.media/-media/index.md). |
| [playIfPaused](../../mozilla.components.feature.media.ext/play-if-paused.md) | `fun `[`MediaState`](./index.md)`.playIfPaused(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.Paused](-paused/index.md) then resume playing all paused [Media](../../mozilla.components.concept.engine.media/-media/index.md). |

### Inheritors

| Name | Summary |
|---|---|
| [None](-none/index.md) | `object None : `[`MediaState`](./index.md)<br>None: No media state. |
| [Paused](-paused/index.md) | `data class Paused : `[`MediaState`](./index.md)<br>Paused: [media](-paused/media.md) of [session](-paused/session.md) is currently paused. |
| [Playing](-playing/index.md) | `data class Playing : `[`MediaState`](./index.md)<br>Playing: [media](-playing/media.md) of [session](-playing/session.md) is currently playing. |
