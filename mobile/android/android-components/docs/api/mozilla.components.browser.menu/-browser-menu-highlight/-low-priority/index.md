[android-components](../../../index.md) / [mozilla.components.browser.menu](../../index.md) / [BrowserMenuHighlight](../index.md) / [LowPriority](./index.md)

# LowPriority

`data class LowPriority : `[`BrowserMenuHighlight`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/BrowserMenuHighlight.kt#L39)

Displays a notification dot.
Used for highlighting new features to the user, such as what's new or a recommended feature.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LowPriority(notificationTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Displays a notification dot. Used for highlighting new features to the user, such as what's new or a recommended feature. |

### Properties

| Name | Summary |
|---|---|
| [canPropagate](can-propagate.md) | `val canPropagate: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicate whether other components should consider this highlight when displaying their own highlight. |
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Label to override the normal label of the menu item. |
| [notificationTint](notification-tint.md) | `val notificationTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Tint for the notification dot displayed on the icon and menu button. |

### Functions

| Name | Summary |
|---|---|
| [asEffect](as-effect.md) | `fun asEffect(context: <ERROR CLASS>): `[`LowPriorityHighlightEffect`](../../../mozilla.components.concept.menu.candidate/-low-priority-highlight-effect/index.md)<br>Converts the highlight into a corresponding [MenuEffect](../../../mozilla.components.concept.menu.candidate/-menu-effect.md) from concept-menu. |
