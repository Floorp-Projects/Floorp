[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [EngineAction](./index.md)

# EngineAction

`sealed class EngineAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L248)

[BrowserAction](../-browser-action.md) implementations related to updating the [EngineState](../../mozilla.components.browser.state.state/-engine-state/index.md) of a single [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) inside
[BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [LinkEngineSessionAction](-link-engine-session-action/index.md) | `data class LinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Attaches the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](-link-engine-session-action/session-id.md). |
| [UnlinkEngineSessionAction](-unlink-engine-session-action/index.md) | `data class UnlinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Detaches the current [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) from the session with the provided [sessionId](-unlink-engine-session-action/session-id.md). |
| [UpdateEngineSessionStateAction](-update-engine-session-state-action/index.md) | `data class UpdateEngineSessionStateAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](-update-engine-session-state-action/session-id.md). |

### Inheritors

| Name | Summary |
|---|---|
| [LinkEngineSessionAction](-link-engine-session-action/index.md) | `data class LinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Attaches the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to the session with the provided [sessionId](-link-engine-session-action/session-id.md). |
| [UnlinkEngineSessionAction](-unlink-engine-session-action/index.md) | `data class UnlinkEngineSessionAction : `[`EngineAction`](./index.md)<br>Detaches the current [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) from the session with the provided [sessionId](-unlink-engine-session-action/session-id.md). |
| [UpdateEngineSessionStateAction](-update-engine-session-state-action/index.md) | `data class UpdateEngineSessionStateAction : `[`EngineAction`](./index.md)<br>Updates the [EngineSessionState](../../mozilla.components.concept.engine/-engine-session-state/index.md) of the session with the provided [sessionId](-update-engine-session-state-action/session-id.md). |
