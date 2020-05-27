[android-components](../../index.md) / [mozilla.components.browser.menu.item](../index.md) / [BrowserMenuItemToolbar](index.md) / [interactiveCount](./interactive-count.md)

# interactiveCount

`val interactiveCount: () -> `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuItemToolbar.kt#L35)

Overrides [BrowserMenuItem.interactiveCount](../../mozilla.components.browser.menu/-browser-menu-item/interactive-count.md)

Lambda expression that returns the number of interactive elements in this menu item.
For example, a simple item will have 1, divider will have 0, and a composite
item, like a tool bar, will have several.

