[android-components](../../../index.md) / [mozilla.components.feature.media.state](../../index.md) / [MediaState](../index.md) / [Paused](./index.md)

# Paused

`data class Paused : `[`MediaState`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/state/MediaState.kt#L38)

Paused: [media](media.md) of [session](session.md) is currently paused.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Paused(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`, media: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Media`](../../../mozilla.components.concept.engine.media/-media/index.md)`>)`<br>Paused: [media](media.md) of [session](session.md) is currently paused. |

### Properties

| Name | Summary |
|---|---|
| [media](media.md) | `val media: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Media`](../../../mozilla.components.concept.engine.media/-media/index.md)`>`<br>The paused [Media](../../../mozilla.components.concept.engine.media/-media/index.md) of the [Session](../../../mozilla.components.browser.session/-session/index.md). |
| [session](session.md) | `val session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)<br>The [Session](../../../mozilla.components.browser.session/-session/index.md) with currently paused media. |
