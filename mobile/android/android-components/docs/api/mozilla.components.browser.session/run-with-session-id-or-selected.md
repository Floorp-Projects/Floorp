[android-components](../index.md) / [mozilla.components.browser.session](index.md) / [runWithSessionIdOrSelected](./run-with-session-id-or-selected.md)

# runWithSessionIdOrSelected

`fun `[`SessionManager`](-session-manager/index.md)`.runWithSessionIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L392)

Tries to find a session with the provided session ID or uses the selected session and runs the block if found.

**Return**
True if the session was found and run successfully.

