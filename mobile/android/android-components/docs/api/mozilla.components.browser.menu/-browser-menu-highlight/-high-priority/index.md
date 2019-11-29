[android-components](../../../index.md) / [mozilla.components.browser.menu](../../index.md) / [BrowserMenuHighlight](../index.md) / [HighPriority](./index.md)

# HighPriority

`data class HighPriority : `[`BrowserMenuHighlight`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L40)

Changes the background of the menu item.
Used for errors that require user attention, like sync errors.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HighPriority(backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID)`<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |

### Properties

| Name | Summary |
|---|---|
| [backgroundTint](background-tint.md) | `val backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Tint for the menu item background color. Also used to highlight the menu button. |
| [endImageResource](end-image-resource.md) | `val endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Icon to display at the end of the menu item when highlighted. |
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Label to override the normal label of the menu item. |
