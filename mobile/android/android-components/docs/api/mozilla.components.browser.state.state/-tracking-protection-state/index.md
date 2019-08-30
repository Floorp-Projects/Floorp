[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TrackingProtectionState](./index.md)

# TrackingProtectionState

`data class TrackingProtectionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TrackingProtectionState.kt#L17)

Value type that represents the state of tracking protection within a [SessionState](../-session-state/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackingProtectionState(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, blockedTrackers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`> = emptyList(), loadedTrackers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`> = emptyList())`<br>Value type that represents the state of tracking protection within a [SessionState](../-session-state/index.md). |

### Properties

| Name | Summary |
|---|---|
| [blockedTrackers](blocked-trackers.md) | `val blockedTrackers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>`<br>List of trackers that have been blocked for the currently loaded site. |
| [enabled](enabled.md) | `val enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether tracking protection is enabled or not for this [SessionState](../-session-state/index.md). |
| [loadedTrackers](loaded-trackers.md) | `val loadedTrackers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>`<br>List of trackers that have been loaded (not blocked) for the currently loaded site. |
