[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsAdapter](index.md) / [updateSessions](./update-sessions.md)

# updateSessions

`fun updateSessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsAdapter.kt#L64)

Overrides [TabsTray.updateSessions](../../mozilla.components.concept.tabstray/-tabs-tray/update-sessions.md)

Updates the list of sessions.

Calling this method is usually followed by calling onSession*() methods to indicate what
exactly has changed. This allows the tabs tray implementation to animate between the old and
new state.

### Parameters

`sessions` - The list of sessions to be displayed.

`selectedIndex` - The list index of the currently selected session.