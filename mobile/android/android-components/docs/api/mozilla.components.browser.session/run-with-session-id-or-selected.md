[android-components](../index.md) / [mozilla.components.browser.session](index.md) / [runWithSessionIdOrSelected](./run-with-session-id-or-selected.md)

# runWithSessionIdOrSelected

`fun `[`SessionManager`](-session-manager/index.md)`.runWithSessionIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, block: `[`SessionManager`](-session-manager/index.md)`.(`[`Session`](-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L530)

Tries to find a session with the provided session ID or uses the selected session and runs the block if found.

**Return**
The result of the [block](run-with-session-id-or-selected.md#mozilla.components.browser.session$runWithSessionIdOrSelected(mozilla.components.browser.session.SessionManager, kotlin.String, kotlin.Function2((mozilla.components.browser.session.SessionManager, mozilla.components.browser.session.Session, kotlin.Boolean)))/block) executed.

