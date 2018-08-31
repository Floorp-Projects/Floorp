---
title: SessionManager.Observer.onAllSessionsRemoved - 
---

[mozilla.components.browser.session](../../index.html) / [SessionManager](../index.html) / [Observer](index.html) / [onAllSessionsRemoved](./on-all-sessions-removed.html)

# onAllSessionsRemoved

`open fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

All sessions have been removed. Note that this will callback will be invoked whenever
removeAll() or removeSessions have been called on the
SessionManager. This callback will NOT be invoked when just the last
session has been removed by calling remove() on the SessionManager.

