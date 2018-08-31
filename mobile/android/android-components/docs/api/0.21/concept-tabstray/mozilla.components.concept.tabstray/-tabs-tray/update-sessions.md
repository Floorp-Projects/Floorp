---
title: TabsTray.updateSessions - 
---

[mozilla.components.concept.tabstray](../index.html) / [TabsTray](index.html) / [updateSessions](./update-sessions.html)

# updateSessions

`abstract fun updateSessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<Session>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Updates the list of sessions.

Calling this method is usually followed by calling onSession*() methods to indicate what
exactly has changed. This allows the tabs tray implementation to animate between the old and
new state.

### Parameters

`sessions` - The list of sessions to be displayed.

`selectedIndex` - The list index of the currently selected session.