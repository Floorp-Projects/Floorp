[android-components](../../index.md) / [mozilla.components.concept.tabstray](../index.md) / [TabsTray](index.md) / [displaySessions](./display-sessions.md)

# displaySessions

`abstract fun displaySessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/TabsTray.kt#L38)

Displays the given list of sessions.

This method will be invoked with the initial list of sessions that should be displayed.

### Parameters

`sessions` - The list of sessions to be displayed.

`selectedIndex` - The list index of the currently selected session.