[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [TrackingProtectionAction](./index.md)

# TrackingProtectionAction

`sealed class TrackingProtectionAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L222)

[BrowserAction](../-browser-action.md) implementations related to updating the [TrackingProtectionState](../../mozilla.components.browser.state.state/-tracking-protection-state/index.md) of a single [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) inside
[BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [ClearTrackersAction](-clear-trackers-action/index.md) | `data class ClearTrackersAction : `[`TrackingProtectionAction`](./index.md)<br>Clears the [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) and [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) lists. |
| [ToggleAction](-toggle-action/index.md) | `data class ToggleAction : `[`TrackingProtectionAction`](./index.md)<br>Updates the [TrackingProtectionState.enabled](../../mozilla.components.browser.state.state/-tracking-protection-state/enabled.md) flag. |
| [TrackerBlockedAction](-tracker-blocked-action/index.md) | `data class TrackerBlockedAction : `[`TrackingProtectionAction`](./index.md)<br>Adds a [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) list. |
| [TrackerLoadedAction](-tracker-loaded-action/index.md) | `data class TrackerLoadedAction : `[`TrackingProtectionAction`](./index.md)<br>Adds a [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.loadedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/loaded-trackers.md) list. |

### Inheritors

| Name | Summary |
|---|---|
| [ClearTrackersAction](-clear-trackers-action/index.md) | `data class ClearTrackersAction : `[`TrackingProtectionAction`](./index.md)<br>Clears the [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) and [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) lists. |
| [ToggleAction](-toggle-action/index.md) | `data class ToggleAction : `[`TrackingProtectionAction`](./index.md)<br>Updates the [TrackingProtectionState.enabled](../../mozilla.components.browser.state.state/-tracking-protection-state/enabled.md) flag. |
| [TrackerBlockedAction](-tracker-blocked-action/index.md) | `data class TrackerBlockedAction : `[`TrackingProtectionAction`](./index.md)<br>Adds a [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.blockedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/blocked-trackers.md) list. |
| [TrackerLoadedAction](-tracker-loaded-action/index.md) | `data class TrackerLoadedAction : `[`TrackingProtectionAction`](./index.md)<br>Adds a [Tracker](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md) to the [TrackingProtectionState.loadedTrackers](../../mozilla.components.browser.state.state/-tracking-protection-state/loaded-trackers.md) list. |
