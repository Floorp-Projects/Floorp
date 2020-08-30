[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [EngineAction](./index.md)

# EngineAction

`sealed class EngineAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L454)

[BrowserAction](../-browser-action.md) implementations related to updating the [EngineState](../../mozilla.components.browser.state.state/-engine-state/index.md) of a single [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) inside
[BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [ClearDataAction](-clear-data-action/index.md) | `data class ClearDataAction : `[`EngineAction`](./index.md)<br>Clears browsing data for the tab with the given [sessionId](-clear-data-action/session-id.md). |
| [CreateEngineSessionAction](-create-engine-session-action/index.md) | `data class CreateEngineSessionAction : `[`EngineAction`](./index.md)<br>Creates an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the given [tabId](-create-engine-session-action/tab-id.md) if none exists yet. |
| [ExitFullScreenModeAction](-exit-full-screen-mode-action/index.md) | `data class ExitFullScreenModeAction : `[`EngineAction`](./index.md)<br>Exits fullscreen mode in the tabs with the given [sessionId](-exit-full-screen-mode-action/session-id.md). |
| [GoBackAction](-go-back-action/index.md) | `data class GoBackAction : `[`EngineAction`](./index.md)<br>Navigates back in the tab with the given [sessionId](-go-back-action/session-id.md). |
| [GoForwardAction](-go-forward-action/index.md) | `data class GoForwardAction : `[`EngineAction`](./index.md)<br>Navigates forward in the tab with the given [sessionId](-go-forward-action/session-id.md). |
| [GoToHistoryIndexAction](-go-to-history-index-action/index.md) | `data class GoToHistoryIndexAction : `[`EngineAction`](./index.md)<br>Navigates to the specified index in the history of the tab with the given [sessionId](-go-to-history-index-action/session-id.md). |
| [LinkEngineSessionAction](-link-engine-session-action/index.md) | `data class LinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Attaches the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](-link-engine-session-action/session-id.md). |
| [LoadDataAction](-load-data-action/index.md) | `data class LoadDataAction : `[`EngineAction`](./index.md)<br>Loads [data](-load-data-action/data.md) in the tab with the given [sessionId](-load-data-action/session-id.md). |
| [LoadUrlAction](-load-url-action/index.md) | `data class LoadUrlAction : `[`EngineAction`](./index.md)<br>Loads the given [url](-load-url-action/url.md) in the tab with the given [sessionId](-load-url-action/session-id.md). |
| [ReloadAction](-reload-action/index.md) | `data class ReloadAction : `[`EngineAction`](./index.md)<br>Reloads the tab with the given [sessionId](-reload-action/session-id.md). |
| [SuspendEngineSessionAction](-suspend-engine-session-action/index.md) | `data class SuspendEngineSessionAction : `[`EngineAction`](./index.md)<br>Suspends the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) of the session with the provided [sessionId](-suspend-engine-session-action/session-id.md). |
| [ToggleDesktopModeAction](-toggle-desktop-mode-action/index.md) | `data class ToggleDesktopModeAction : `[`EngineAction`](./index.md)<br>Enables/disables desktop mode in the tabs with the given [sessionId](-toggle-desktop-mode-action/session-id.md). |
| [UnlinkEngineSessionAction](-unlink-engine-session-action/index.md) | `data class UnlinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Detaches the current [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) from the session with the provided [sessionId](-unlink-engine-session-action/session-id.md). |
| [UpdateEngineSessionObserverAction](-update-engine-session-observer-action/index.md) | `data class UpdateEngineSessionObserverAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSession.Observer](../../mozilla.components.concept.engine/-engine-session/-observer/index.md) of the session with the provided [sessionId](-update-engine-session-observer-action/session-id.md). |
| [UpdateEngineSessionStateAction](-update-engine-session-state-action/index.md) | `data class UpdateEngineSessionStateAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](-update-engine-session-state-action/session-id.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ClearDataAction](-clear-data-action/index.md) | `data class ClearDataAction : `[`EngineAction`](./index.md)<br>Clears browsing data for the tab with the given [sessionId](-clear-data-action/session-id.md). |
| [CreateEngineSessionAction](-create-engine-session-action/index.md) | `data class CreateEngineSessionAction : `[`EngineAction`](./index.md)<br>Creates an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) for the given [tabId](-create-engine-session-action/tab-id.md) if none exists yet. |
| [ExitFullScreenModeAction](-exit-full-screen-mode-action/index.md) | `data class ExitFullScreenModeAction : `[`EngineAction`](./index.md)<br>Exits fullscreen mode in the tabs with the given [sessionId](-exit-full-screen-mode-action/session-id.md). |
| [GoBackAction](-go-back-action/index.md) | `data class GoBackAction : `[`EngineAction`](./index.md)<br>Navigates back in the tab with the given [sessionId](-go-back-action/session-id.md). |
| [GoForwardAction](-go-forward-action/index.md) | `data class GoForwardAction : `[`EngineAction`](./index.md)<br>Navigates forward in the tab with the given [sessionId](-go-forward-action/session-id.md). |
| [GoToHistoryIndexAction](-go-to-history-index-action/index.md) | `data class GoToHistoryIndexAction : `[`EngineAction`](./index.md)<br>Navigates to the specified index in the history of the tab with the given [sessionId](-go-to-history-index-action/session-id.md). |
| [LinkEngineSessionAction](-link-engine-session-action/index.md) | `data class LinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Attaches the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](-link-engine-session-action/session-id.md). |
| [LoadDataAction](-load-data-action/index.md) | `data class LoadDataAction : `[`EngineAction`](./index.md)<br>Loads [data](-load-data-action/data.md) in the tab with the given [sessionId](-load-data-action/session-id.md). |
| [LoadUrlAction](-load-url-action/index.md) | `data class LoadUrlAction : `[`EngineAction`](./index.md)<br>Loads the given [url](-load-url-action/url.md) in the tab with the given [sessionId](-load-url-action/session-id.md). |
| [ReloadAction](-reload-action/index.md) | `data class ReloadAction : `[`EngineAction`](./index.md)<br>Reloads the tab with the given [sessionId](-reload-action/session-id.md). |
| [SuspendEngineSessionAction](-suspend-engine-session-action/index.md) | `data class SuspendEngineSessionAction : `[`EngineAction`](./index.md)<br>Suspends the [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) of the session with the provided [sessionId](-suspend-engine-session-action/session-id.md). |
| [ToggleDesktopModeAction](-toggle-desktop-mode-action/index.md) | `data class ToggleDesktopModeAction : `[`EngineAction`](./index.md)<br>Enables/disables desktop mode in the tabs with the given [sessionId](-toggle-desktop-mode-action/session-id.md). |
| [UnlinkEngineSessionAction](-unlink-engine-session-action/index.md) | `data class UnlinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Detaches the current [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) from the session with the provided [sessionId](-unlink-engine-session-action/session-id.md). |
| [UpdateEngineSessionObserverAction](-update-engine-session-observer-action/index.md) | `data class UpdateEngineSessionObserverAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSession.Observer](../../mozilla.components.concept.engine/-engine-session/-observer/index.md) of the session with the provided [sessionId](-update-engine-session-observer-action/session-id.md). |
| [UpdateEngineSessionStateAction](-update-engine-session-state-action/index.md) | `data class UpdateEngineSessionStateAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](-update-engine-session-state-action/session-id.md). |
