[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SessionManager](index.md) / [add](./add.md)

# add

`fun add(session: `[`Session`](../-session/index.md)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, engineSession: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`? = null, engineSessionState: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`? = null, parent: `[`Session`](../-session/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L189)

Adds the provided session.

`fun add(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../-session/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L239)

Adds multiple sessions.

Note that for performance reasons this method will invoke
[SessionManager.Observer.onSessionsRestored](-observer/on-sessions-restored.md) and not [SessionManager.Observer.onSessionAdded](-observer/on-session-added.md)
for every added [Session](../-session/index.md).

