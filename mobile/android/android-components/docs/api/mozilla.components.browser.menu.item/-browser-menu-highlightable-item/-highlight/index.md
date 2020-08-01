[android-components](../../../index.md) / [mozilla.components.browser.menu.item](../../index.md) / [BrowserMenuHighlightableItem](../index.md) / [Highlight](./index.md)

# Highlight

`class ~~Highlight~~ : `[`ClassicHighlight`](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/item/BrowserMenuHighlightableItem.kt#L180)
**Deprecated:** Replace with BrowserMenuHighlight.LowPriority or BrowserMenuHighlight.HighPriority

Described how to display a [BrowserMenuHighlightableItem](../index.md) when it is highlighted.
Replaced by [BrowserMenuHighlight](../../../mozilla.components.browser.menu/-browser-menu-highlight/index.md) which lets a priority be specified.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Highlight(startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, backgroundResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, colorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Described how to display a [BrowserMenuHighlightableItem](../index.md) when it is highlighted. Replaced by [BrowserMenuHighlight](../../../mozilla.components.browser.menu/-browser-menu-highlight/index.md) which lets a priority be specified. |

### Inherited Properties

| Name | Summary |
|---|---|
| [backgroundResource](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/background-resource.md) | `val backgroundResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [canPropagate](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/can-propagate.md) | `open val canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicate whether other components should consider this highlight when displaying their own highlight. |
| [colorResource](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/color-resource.md) | `val colorResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [endImageResource](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/end-image-resource.md) | `val endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [label](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/label.md) | `open val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [startImageResource](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/start-image-resource.md) | `val startImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [asEffect](../../../mozilla.components.browser.menu/-browser-menu-highlight/-classic-highlight/as-effect.md) | `open fun asEffect(context: <ERROR CLASS>): `[`HighPriorityHighlightEffect`](../../../mozilla.components.concept.menu.candidate/-high-priority-highlight-effect/index.md)<br>Converts the highlight into a corresponding [MenuEffect](../../../mozilla.components.concept.menu.candidate/-menu-effect.md) from concept-menu. |
