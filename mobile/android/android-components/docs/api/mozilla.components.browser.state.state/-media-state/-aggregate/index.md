[android-components](../../../index.md) / [mozilla.components.browser.state.state](../../index.md) / [MediaState](../index.md) / [Aggregate](./index.md)

# Aggregate

`data class Aggregate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/MediaState.kt#L71)

Value type representing the aggregated "global" media state.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Aggregate(state: `[`State`](../-state/index.md)` = State.NONE, activeTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, activeMedia: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = emptyList(), activeFullscreenOrientation: `[`FullscreenOrientation`](../-fullscreen-orientation/index.md)`? = null)`<br>Value type representing the aggregated "global" media state. |

### Properties

| Name | Summary |
|---|---|
| [activeFullscreenOrientation](active-fullscreen-orientation.md) | `val activeFullscreenOrientation: `[`FullscreenOrientation`](../-fullscreen-orientation/index.md)`?`<br>The current predicted orientation of active fullscreen media (or null). |
| [activeMedia](active-media.md) | `val activeMedia: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>A list of ids (String) of currently active media elements on the active tab. |
| [activeTabId](active-tab-id.md) | `val activeTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The ID of the currently active tab (or null). Can be the id of a regular tab or a custom tab. |
| [state](state.md) | `val state: `[`State`](../-state/index.md)<br>The aggregated [State](../-state/index.md) for all tabs and media combined. |
