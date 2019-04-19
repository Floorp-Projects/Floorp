[android-components](../../index.md) / [mozilla.components.browser.session.state](../index.md) / [BrowserState](./index.md)

# BrowserState

`data class BrowserState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/state/BrowserState.kt#L13)

Value type that represents the complete state of the browser/engine.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserState(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SessionState`](../-session-state/index.md)`> = emptyList(), selectedSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Value type that represents the complete state of the browser/engine. |

### Properties

| Name | Summary |
|---|---|
| [selectedSessionId](selected-session-id.md) | `val selectedSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The ID of the currently selected session (present in the [sessions](sessions.md) list]. |
| [sessions](sessions.md) | `val sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SessionState`](../-session-state/index.md)`>`<br>List of currently open sessions ("tabs"). |

### Extension Properties

| Name | Summary |
|---|---|
| [selectedSessionState](../../mozilla.components.browser.session.selector/selected-session-state.md) | `val `[`BrowserState`](./index.md)`.selectedSessionState: `[`SessionState`](../-session-state/index.md)`?`<br>The currently selected [SessionState](../-session-state/index.md) if there's one. |

### Extension Functions

| Name | Summary |
|---|---|
| [findSession](../../mozilla.components.browser.session.selector/find-session.md) | `fun `[`BrowserState`](./index.md)`.findSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SessionState`](../-session-state/index.md)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [findSessionOrSelected](../../mozilla.components.browser.session.selector/find-session-or-selected.md) | `fun `[`BrowserState`](./index.md)`.findSessionOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`SessionState`](../-session-state/index.md)`?`<br>Finds and returns the session with the given id or the selected session if no id was provided (null). Returns null if no matching session could be found or if no selected session exists. |
