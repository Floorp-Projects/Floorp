[android-components](../../index.md) / [mozilla.components.browser.toolbar.display](../index.md) / [DisplayToolbarView](./index.md)

# DisplayToolbarView

`class DisplayToolbarView : ConstraintLayout` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/display/DisplayToolbarView.kt#L19)

Custom ConstraintLayout for DisplayToolbar that allows us to draw ripple backgrounds on the toolbar
by setting a background to transparent.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DisplayToolbarView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>Custom ConstraintLayout for DisplayToolbar that allows us to draw ripple backgrounds on the toolbar by setting a background to transparent. |

### Properties

| Name | Summary |
|---|---|
| [backgroundView](background-view.md) | `lateinit var backgroundView: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [draw](draw.md) | `fun draw(canvas: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onFinishInflate](on-finish-inflate.md) | `fun onFinishInflate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
