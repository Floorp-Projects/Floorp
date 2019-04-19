[android-components](../index.md) / [mozilla.components.browser.session.selector](./index.md)

## Package mozilla.components.browser.session.selector

### Properties

| Name | Summary |
|---|---|
| [selectedSessionState](selected-session-state.md) | `val `[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`.selectedSessionState: `[`SessionState`](../mozilla.components.browser.session.state/-session-state/index.md)`?`<br>The currently selected [SessionState](../mozilla.components.browser.session.state/-session-state/index.md) if there's one. |

### Functions

| Name | Summary |
|---|---|
| [findSession](find-session.md) | `fun `[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`.findSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SessionState`](../mozilla.components.browser.session.state/-session-state/index.md)`?`<br>Finds and returns the session with the given id. Returns null if no matching session could be found. |
| [findSessionOrSelected](find-session-or-selected.md) | `fun `[`BrowserState`](../mozilla.components.browser.session.state/-browser-state/index.md)`.findSessionOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`SessionState`](../mozilla.components.browser.session.state/-session-state/index.md)`?`<br>Finds and returns the session with the given id or the selected session if no id was provided (null). Returns null if no matching session could be found or if no selected session exists. |
