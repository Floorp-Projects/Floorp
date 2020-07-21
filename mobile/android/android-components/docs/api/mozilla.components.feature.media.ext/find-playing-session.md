[android-components](../index.md) / [mozilla.components.feature.media.ext](index.md) / [findPlayingSession](./find-playing-session.md)

# findPlayingSession

`fun `[`MediaState`](../mozilla.components.browser.state.state/-media-state/index.md)`.findPlayingSession(): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Element`](../mozilla.components.browser.state.state/-media-state/-element/index.md)`>>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/ext/MediaState.kt#L55)

Find a [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) with playing [Media](../mozilla.components.concept.engine.media/-media/index.md) and return this [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) or `null` if no such
[SessionState](../mozilla.components.browser.state.state/-session-state/index.md) could be found.

