[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [MediaState](./index.md)

# MediaState

`data class MediaState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/MediaState.kt#L18)

Value type that represents the state of the media elements and playback states.

### Types

| Name | Summary |
|---|---|
| [Aggregate](-aggregate/index.md) | `data class Aggregate`<br>Value type representing the aggregated "global" media state. |
| [Element](-element/index.md) | `data class Element`<br>Value type representing a media element on a website. |
| [FullscreenOrientation](-fullscreen-orientation/index.md) | `enum class FullscreenOrientation`<br>Enum of predicted media screen orientation. |
| [State](-state/index.md) | `enum class State`<br>Enum of "global" aggregated media state. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MediaState(aggregate: `[`Aggregate`](-aggregate/index.md)` = Aggregate(), elements: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](-element/index.md)`>> = emptyMap())`<br>Value type that represents the state of the media elements and playback states. |

### Properties

| Name | Summary |
|---|---|
| [aggregate](aggregate.md) | `val aggregate: `[`Aggregate`](-aggregate/index.md)<br>The "global" aggregated media state. |
| [elements](elements.md) | `val elements: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](-element/index.md)`>>`<br>A map from tab ids to a list of media elements on that tab. |

### Extension Functions

| Name | Summary |
|---|---|
| [findPlayingSession](../../mozilla.components.feature.media.ext/find-playing-session.md) | `fun `[`MediaState`](./index.md)`.findPlayingSession(): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](-element/index.md)`>>?`<br>Find a [SessionState](../-session-state/index.md) with playing [Media](../../mozilla.components.concept.engine.media/-media/index.md) and return this [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) or `null` if no such [SessionState](../-session-state/index.md) could be found. |
| [getActiveElements](../../mozilla.components.feature.media.ext/get-active-elements.md) | `fun `[`MediaState`](./index.md)`.getActiveElements(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](-element/index.md)`>`<br>Returns the list of active [MediaState.Element](-element/index.md)s. |
| [getPausedMedia](../../mozilla.components.feature.media.ext/get-paused-media.md) | `fun `[`MediaState`](./index.md)`.getPausedMedia(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](-element/index.md)`>`<br>Get the list of paused [Media](../../mozilla.components.concept.engine.media/-media/index.md) for the tab with the provided [tabId](../../mozilla.components.feature.media.ext/get-paused-media.md#mozilla.components.feature.media.ext$getPausedMedia(mozilla.components.browser.state.state.MediaState, kotlin.String)/tabId). |
| [getPlayingMediaIdsForTab](../../mozilla.components.feature.media.ext/get-playing-media-ids-for-tab.md) | `fun `[`MediaState`](./index.md)`.getPlayingMediaIdsForTab(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>Returns a list of [MediaState.Element.id](-element/id.md) of playing media on the tab with the given [tabId](../../mozilla.components.feature.media.ext/get-playing-media-ids-for-tab.md#mozilla.components.feature.media.ext$getPlayingMediaIdsForTab(mozilla.components.browser.state.state.MediaState, kotlin.String)/tabId). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [pauseIfPlaying](../../mozilla.components.feature.media.ext/pause-if-playing.md) | `fun `[`MediaState`](./index.md)`.pauseIfPlaying(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.State.PLAYING](-state/-p-l-a-y-i-n-g.md) then pause all playing [Media](../../mozilla.components.concept.engine.media/-media/index.md). |
| [playIfPaused](../../mozilla.components.feature.media.ext/play-if-paused.md) | `fun `[`MediaState`](./index.md)`.playIfPaused(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>If this state is [MediaState.State.PAUSED](-state/-p-a-u-s-e-d.md) then resume playing all paused [Media](../../mozilla.components.concept.engine.media/-media/index.md). |
| [playing](../../mozilla.components.feature.media.ext/playing.md) | `fun `[`MediaState`](./index.md)`.playing(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If this state is [MediaState.State.PLAYING](-state/-p-l-a-y-i-n-g.md) then return true, else return false. |
