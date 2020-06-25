[android-components](../../../index.md) / [mozilla.components.browser.menu](../../index.md) / [BrowserMenuHighlight](../index.md) / [ClassicHighlight](./index.md)

# ClassicHighlight

`open class ~~ClassicHighlight~~ : `[`BrowserMenuHighlight`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L81)
**Deprecated:** Replace with LowPriority or HighPriority highlight

Described how to display a highlightable menu item when it is highlighted.
Replaced by [LowPriority](../-low-priority/index.md) and [HighPriority](../-high-priority/index.md) which lets a priority be specified.
This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight](../../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/-highlight/index.md)
can subclass it.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ClassicHighlight(startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, backgroundResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, colorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Described how to display a highlightable menu item when it is highlighted. Replaced by [LowPriority](../-low-priority/index.md) and [HighPriority](../-high-priority/index.md) which lets a priority be specified. This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight](../../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/-highlight/index.md) can subclass it. |

### Properties

| Name | Summary |
|---|---|
| [backgroundResource](background-resource.md) | `val backgroundResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [canPropagate](can-propagate.md) | `open val canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicate whether other components should consider this highlight when displaying their own highlight. |
| [colorResource](color-resource.md) | `val colorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [endImageResource](end-image-resource.md) | `val endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [label](label.md) | `open val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [startImageResource](start-image-resource.md) | `val startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| Name | Summary |
|---|---|
| [asEffect](as-effect.md) | `open fun asEffect(context: <ERROR CLASS>): `[`HighPriorityHighlightEffect`](../../../mozilla.components.concept.menu.candidate/-high-priority-highlight-effect/index.md)<br>Converts the highlight into a corresponding [MenuEffect](../../../mozilla.components.concept.menu.candidate/-menu-effect.md) from concept-menu. |

### Inheritors

| Name | Summary |
|---|---|
| [Highlight](../../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/-highlight/index.md) | `class ~~Highlight~~ : `[`ClassicHighlight`](./index.md)<br>Described how to display a [BrowserMenuHighlightableItem](../../../mozilla.components.browser.menu.item/-browser-menu-highlightable-item/index.md) when it is highlighted. Replaced by [BrowserMenuHighlight](../index.md) which lets a priority be specified. |
