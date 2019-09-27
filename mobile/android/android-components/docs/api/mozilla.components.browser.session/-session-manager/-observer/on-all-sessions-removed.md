[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [SessionManager](../index.md) / [Observer](index.md) / [onAllSessionsRemoved](./on-all-sessions-removed.md)

# onAllSessionsRemoved

`open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L366)

All sessions have been removed. Note that this will callback will be invoked whenever
removeAll() or removeSessions have been called on the
SessionManager. This callback will NOT be invoked when just the last
session has been removed by calling remove() on the SessionManager.

