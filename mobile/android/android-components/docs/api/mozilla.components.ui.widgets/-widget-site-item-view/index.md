[android-components](../../index.md) / [mozilla.components.ui.widgets](../index.md) / [WidgetSiteItemView](./index.md)

# WidgetSiteItemView

`class WidgetSiteItemView : ConstraintLayout` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/widgets/src/main/java/mozilla/components/ui/widgets/WidgetSiteItemView.kt#L26)

Shared UI widget for showing a website in a list of websites,
such as in bookmarks, history, site exceptions, or collections.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WidgetSiteItemView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>Shared UI widget for showing a website in a list of websites, such as in bookmarks, history, site exceptions, or collections. |

### Properties

| Name | Summary |
|---|---|
| [iconView](icon-view.md) | `val iconView: <ERROR CLASS>`<br>ImageView that should display favicons. |

### Functions

| Name | Summary |
|---|---|
| [addIconOverlay](add-icon-overlay.md) | `fun addIconOverlay(overlay: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a view that will overlay the favicon, such as a checkmark. |
| [removeSecondaryButton](remove-secondary-button.md) | `fun removeSecondaryButton(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the secondary button if it was previously set in [setSecondaryButton](set-secondary-button.md). |
| [setSecondaryButton](set-secondary-button.md) | `fun setSecondaryButton(icon: <ERROR CLASS>?, contentDescription: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`, onClickListener: (<ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`fun setSecondaryButton(icon: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, onClickListener: (<ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a secondary button, such as an overflow menu. |
| [setText](set-text.md) | `fun setText(label: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`, caption: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the text displayed inside of the site item view. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
