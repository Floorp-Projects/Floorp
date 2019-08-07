[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarPresenter](index.md) / [onAllSessionsRemoved](./on-all-sessions-removed.md)

# onAllSessionsRemoved

`fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarPresenter.kt#L55)

Overrides [Observer.onAllSessionsRemoved](../../mozilla.components.browser.session/-session-manager/-observer/on-all-sessions-removed.md)

All sessions have been removed. Note that this will callback will be invoked whenever
removeAll() or removeSessions have been called on the
SessionManager. This callback will NOT be invoked when just the last
session has been removed by calling remove() on the SessionManager.

