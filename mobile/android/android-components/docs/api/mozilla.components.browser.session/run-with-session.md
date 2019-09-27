[android-components](../index.md) / [mozilla.components.browser.session](index.md) / [runWithSession](./run-with-session.md)

# runWithSession

`fun `[`SessionManager`](-session-manager/index.md)`.runWithSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L375)

Tries to find a session with the provided session ID and runs the block if found.

**Return**
True if the session was found and run successfully.

