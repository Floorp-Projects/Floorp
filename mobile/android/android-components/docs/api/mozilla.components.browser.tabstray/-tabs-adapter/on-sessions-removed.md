[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsAdapter](index.md) / [onSessionsRemoved](./on-sessions-removed.md)

# onSessionsRemoved

`fun onSessionsRemoved(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsAdapter.kt#L72)

Overrides [TabsTray.onSessionsRemoved](../../mozilla.components.concept.tabstray/-tabs-tray/on-sessions-removed.md)

Called after updateSessions() when count number of sessions are removed from
the given position.

