[android-components](../../index.md) / [mozilla.components.browser.menu](../index.md) / [BrowserMenuHighlight](./index.md)

# BrowserMenuHighlight

`sealed class BrowserMenuHighlight` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L21)

Describes how to display a [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem](../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/index.md)
when it is highlighted.

### Types

| Name | Summary |
|---|---|
| [ClassicHighlight](-classic-highlight/index.md) | `open class ~~ClassicHighlight~~ : `[`BrowserMenuHighlight`](./index.md)<br>Described how to display a highlightable menu item when it is highlighted. Replaced by [LowPriority](-low-priority/index.md) and [HighPriority](-high-priority/index.md) which lets a priority be specified. This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight](../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/-highlight/index.md) can subclass it. |
| [HighPriority](-high-priority/index.md) | `data class HighPriority : `[`BrowserMenuHighlight`](./index.md)<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |
| [LowPriority](-low-priority/index.md) | `data class LowPriority : `[`BrowserMenuHighlight`](./index.md)<br>Displays a notification dot. Used for highlighting new features to the user, such as what's new or a recommended feature. |

### Properties

| Name | Summary |
|---|---|
| [canPropagate](can-propagate.md) | `abstract val canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [label](label.md) | `abstract val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

### Functions

| Name | Summary |
|---|---|
| [asEffect](as-effect.md) | `abstract fun asEffect(context: <ERROR CLASS>): `[`MenuEffect`](../../mozilla.components.concept.menu.candidate/-menu-effect.md)<br>Converts the highlight into a corresponding [MenuEffect](../../mozilla.components.concept.menu.candidate/-menu-effect.md) from concept-menu. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ClassicHighlight](-classic-highlight/index.md) | `open class ~~ClassicHighlight~~ : `[`BrowserMenuHighlight`](./index.md)<br>Described how to display a highlightable menu item when it is highlighted. Replaced by [LowPriority](-low-priority/index.md) and [HighPriority](-high-priority/index.md) which lets a priority be specified. This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight](../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/-highlight/index.md) can subclass it. |
| [HighPriority](-high-priority/index.md) | `data class HighPriority : `[`BrowserMenuHighlight`](./index.md)<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |
| [LowPriority](-low-priority/index.md) | `data class LowPriority : `[`BrowserMenuHighlight`](./index.md)<br>Displays a notification dot. Used for highlighting new features to the user, such as what's new or a recommended feature. |
