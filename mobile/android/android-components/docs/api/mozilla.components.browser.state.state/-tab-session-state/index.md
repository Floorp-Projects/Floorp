[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [TabSessionState](./index.md)

# TabSessionState

`data class TabSessionState : `[`SessionState`](../-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/TabSessionState.kt#L19)

Value type that represents the state of a tab (private or normal).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabSessionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), content: `[`ContentState`](../-content-state/index.md)`, parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Value type that represents the state of a tab (private or normal). |

### Properties

| Name | Summary |
|---|---|
| [content](content.md) | `val content: `[`ContentState`](../-content-state/index.md)<br>the [ContentState](../-content-state/index.md) of this tab. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of this tab and session. |
| [parentId](parent-id.md) | `val parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the parent ID of this tab or null if this tab has no parent. The parent tab is usually the tab that initiated opening this tab (e.g. the user clicked a link with target="_blank" or selected "open in new tab" or a "window.open" was triggered). |
