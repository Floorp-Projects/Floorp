[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [TrackingProtectionAction](../index.md) / [TrackerBlockedAction](./index.md)

# TrackerBlockedAction

`data class TrackerBlockedAction : `[`TrackingProtectionAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L329)

Adds a [Tracker](../../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.blockedTrackers](../../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) list.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TrackerBlockedAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tracker: `[`Tracker`](../../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`)`<br>Adds a [Tracker](../../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.blockedTrackers](../../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) list. |

### Properties

| Name | Summary |
|---|---|
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [tracker](tracker.md) | `val tracker: `[`Tracker`](../../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) |
