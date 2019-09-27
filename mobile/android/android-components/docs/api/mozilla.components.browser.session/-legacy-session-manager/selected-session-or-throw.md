[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [LegacySessionManager](index.md) / [selectedSessionOrThrow](./selected-session-or-throw.md)

# selectedSessionOrThrow

`val selectedSessionOrThrow: `[`Session`](../-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/LegacySessionManager.kt#L118)

Gets the currently selected session or throws an IllegalStateException if no session is
selected.

It's application specific whether a session manager can have no selected session (= no sessions)
or not. In applications that always have at least one session dealing with the nullable
selectedSession property can be cumbersome. In those situations, where you always
expect a session to exist, you can use selectedSessionOrThrow to avoid dealing
with null values.

Only one session can be selected at a given time.

