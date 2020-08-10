[android-components](../../index.md) / [mozilla.components.concept.tabstray](../index.md) / [TabsTray](index.md) / [updateTabs](./update-tabs.md)

# updateTabs

`abstract fun updateTabs(tabs: `[`Tabs`](../-tabs/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/TabsTray.kt#L36)

Updates the list of tabs.

Calling this method is usually followed by calling onTabs*() methods to indicate what
exactly has changed. This allows the tabs tray implementation to animate between the old and
new state.

