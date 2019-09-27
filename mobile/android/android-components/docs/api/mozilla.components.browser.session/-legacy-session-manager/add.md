[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [LegacySessionManager](index.md) / [add](./add.md)

# add

`fun add(session: `[`Session`](../-session/index.md)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null, parent: `[`Session`](../-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L136)

Adds the provided session.

`fun add(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L209)

Adds multiple sessions.

Note that for performance reasons this method will invoke
[SessionManager.Observer.onSessionsRestored](../-session-manager/-observer/on-sessions-restored.md) and not [SessionManager.Observer.onSessionAdded](../-session-manager/-observer/on-session-added.md)
for every added [Session](../-session/index.md).

