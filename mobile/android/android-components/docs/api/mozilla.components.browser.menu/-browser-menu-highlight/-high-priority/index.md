[android-components](../../../index.md) / [mozilla.components.browser.menu](../../index.md) / [BrowserMenuHighlight](../index.md) / [HighPriority](./index.md)

# HighPriority

`data class HighPriority : `[`BrowserMenuHighlight`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L60)

Changes the background of the menu item.
Used for errors that require user attention, like sync errors.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HighPriority(backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = NO_ID, canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |

### Properties

| Name | Summary |
|---|---|
| [backgroundTint](background-tint.md) | `val backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Tint for the menu item background color. Also used to highlight the menu button. |
| [canPropagate](can-propagate.md) | `val canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicate whether other components should consider this highlight when displaying their own highlight. |
| [endImageResource](end-image-resource.md) | `val endImageResource: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Icon to display at the end of the menu item when highlighted. |
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Label to override the normal label of the menu item. |

### Functions

| Name | Summary |
|---|---|
| [asEffect](as-effect.md) | `fun asEffect(context: <ERROR CLASS>): `[`HighPriorityHighlightEffect`](../../../mozilla.components.concept.menu.candidate/-high-priority-highlight-effect/index.md)<br>Converts the highlight into a corresponding [MenuEffect](../../../mozilla.components.concept.menu.candidate/-menu-effect.md) from concept-menu. |
