[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuItem](index.md) / [interactiveCount](./interactive-count.md)

# interactiveCount

`open val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuItem.kt#L26)

Lambda expression that returns the number of interactive elements in this menu item.
For example, a simple item will have 1, divider will have 0, and a composite
item, like a tool bar, will have several.

