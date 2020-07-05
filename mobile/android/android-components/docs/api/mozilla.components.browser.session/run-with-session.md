[android-components](../index.md) / [mozilla.components.browser.session](index.md) / [runWithSession](./run-with-session.md)

# runWithSession

`fun `[`SessionManager`](-session-manager/index.md)`.runWithSession(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L513)

Tries to find a session with the provided session ID and runs the block if found.

**Return**
The result of the [block](run-with-session.md#mozilla.components.browser.session$runWithSession(mozilla.components.browser.session.SessionManager, kotlin.String, kotlin.Function2((mozilla.components.browser.session.SessionManager, mozilla.components.browser.session.Session, kotlin.Boolean)))/block) executed.

