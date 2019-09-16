[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [SessionState](./index.md)

# SessionState

`interface SessionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/SessionState.kt#L15)

Interface for states that contain a [ContentState](../-content-state/index.md) and can be accessed via an [id](id.md).

### Properties

| Name | Summary |
|---|---|
| [content](content.md) | `abstract val content: `[`ContentState`](../-content-state/index.md)<br>the [ContentState](../-content-state/index.md) of this session. |
| [engineState](engine-state.md) | `abstract val engineState: `[`EngineState`](../-engine-state/index.md)<br>the [EngineState](../-engine-state/index.md) of this session. |
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique id of the session. |
| [trackingProtection](tracking-protection.md) | `abstract val trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)<br>the [TrackingProtectionState](../-tracking-protection-state/index.md) of this session. |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabSessionState](../-custom-tab-session-state/index.md) | `data class CustomTabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a Custom Tab. |
| [TabSessionState](../-tab-session-state/index.md) | `data class TabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a tab (private or normal). |
