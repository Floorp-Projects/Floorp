[android-components](../index.md) / [mozilla.components.browser.session](./index.md)

## Package mozilla.components.browser.session

### Types

| Name | Summary |
|---|---|
| [LegacySessionManager](-legacy-session-manager/index.md) | `class LegacySessionManager : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-session-manager/-observer/index.md)`>`<br>This class provides access to a centralized registry of all active sessions. |
| [SelectionAwareSessionObserver](-selection-aware-session-observer/index.md) | `abstract class SelectionAwareSessionObserver : `[`Observer`](-session-manager/-observer/index.md)`, `[`Observer`](-session/-observer/index.md)<br>This class is a combination of [Session.Observer](-session/-observer/index.md) and [SessionManager.Observer](-session-manager/-observer/index.md). It provides functionality to observe changes to a specified or selected session, and can automatically take care of switching over the observer in case a different session gets selected (see [observeFixed](-selection-aware-session-observer/observe-fixed.md) and [observeSelected](-selection-aware-session-observer/observe-selected.md)). |
| [Session](-session/index.md) | `class Session : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-session/-observer/index.md)`>`<br>Value type that represents the state of a browser session. Changes can be observed. |
| [SessionManager](-session-manager/index.md) | `class SessionManager : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-session-manager/-observer/index.md)`>, `[`MemoryConsumer`](../mozilla.components.support.base.memory/-memory-consumer/index.md)<br>This class provides access to a centralized registry of all active sessions. |

### Functions

| Name | Summary |
|---|---|
| [runWithSession](run-with-session.md) | `fun `[`SessionManager`](-session-manager/index.md)`.runWithSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to find a session with the provided session ID and runs the block if found. |
| [runWithSessionIdOrSelected](run-with-session-id-or-selected.md) | `fun `[`SessionManager`](-session-manager/index.md)`.runWithSessionIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to find a session with the provided session ID or uses the selected session and runs the block if found. |
