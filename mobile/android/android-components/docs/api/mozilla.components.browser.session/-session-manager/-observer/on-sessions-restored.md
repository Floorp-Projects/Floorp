[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [SessionManager](../index.md) / [Observer](index.md) / [onSessionsRestored](./on-sessions-restored.md)

# onSessionsRestored

`open fun onSessionsRestored(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L491)

Sessions have been restored via a snapshot. This callback is invoked at the end of the
call to read, after every session in the snapshot was added, and
appropriate session was selected.

