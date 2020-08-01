[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsAdapter](index.md) / [updateTabs](./update-tabs.md)

# updateTabs

`open fun updateTabs(tabs: `[`Tabs`](../../mozilla.components.concept.tabstray/-tabs/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsAdapter.kt#L55)

Overrides [TabsTray.updateTabs](../../mozilla.components.concept.tabstray/-tabs-tray/update-tabs.md)

Updates the list of tabs.

Calling this method is usually followed by calling onTabs*() methods to indicate what
exactly has changed. This allows the tabs tray implementation to animate between the old and
new state.

