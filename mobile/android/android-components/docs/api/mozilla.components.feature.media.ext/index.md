[android-components](../index.md) / [mozilla.components.feature.media.ext](./index.md)

## Package mozilla.components.feature.media.ext

### Functions

| Name | Summary |
|---|---|
| [getSession](get-session.md) | `fun `[`MediaState`](../mozilla.components.feature.media.state/-media-state/index.md)`.getSession(): `[`Session`](../mozilla.components.browser.session/-session/index.md)`?`<br>Get the [Session](../mozilla.components.browser.session/-session/index.md) that caused this [MediaState](../mozilla.components.feature.media.state/-media-state/index.md) (if any). |
| [isForCustomTabSession](is-for-custom-tab-session.md) | `fun `[`MediaState`](../mozilla.components.feature.media.state/-media-state/index.md)`.isForCustomTabSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this [MediaState](../mozilla.components.feature.media.state/-media-state/index.md) is associated with a Custom Tab [Session](../mozilla.components.browser.session/-session/index.md). |
| [pauseIfPlaying](pause-if-playing.md) | `fun `[`MediaState`](../mozilla.components.feature.media.state/-media-state/index.md)`.pauseIfPlaying(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.Playing](../mozilla.components.feature.media.state/-media-state/-playing/index.md) then pause all playing [Media](../mozilla.components.concept.engine.media/-media/index.md). |
| [playIfPaused](play-if-paused.md) | `fun `[`MediaState`](../mozilla.components.feature.media.state/-media-state/index.md)`.playIfPaused(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.Paused](../mozilla.components.feature.media.state/-media-state/-paused/index.md) then resume playing all paused [Media](../mozilla.components.concept.engine.media/-media/index.md). |
