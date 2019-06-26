[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [SessionState](./index.md)

# SessionState

`interface SessionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/SessionState.kt#L6)

Interface for states that contain a [ContentState](../-content-state/index.md) and can be accessed via an [id](id.md).

### Properties

| Name | Summary |
|---|---|
| [content](content.md) | `abstract val content: `[`ContentState`](../-content-state/index.md) |
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabSessionState](../-custom-tab-session-state/index.md) | `data class CustomTabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a Custom Tab. |
| [TabSessionState](../-tab-session-state/index.md) | `data class TabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a tab (private or normal). |
