[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TabSessionState](./index.md)

# TabSessionState

`data class TabSessionState : `[`SessionState`](../-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TabSessionState.kt#L12)

Value type that represents the state of a tab (private or normal).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabSessionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), content: `[`ContentState`](../-content-state/index.md)`)`<br>Value type that represents the state of a tab (private or normal). |

### Properties

| Name | Summary |
|---|---|
| [content](content.md) | `val content: `[`ContentState`](../-content-state/index.md) |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
