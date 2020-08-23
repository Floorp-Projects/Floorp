[android-components](../../index.md) / [mozilla.components.browser.menu.view](../index.md) / [DynamicWidthRecyclerView](./index.md)

# DynamicWidthRecyclerView

`class DynamicWidthRecyclerView : RecyclerView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/menu/src/main/java/mozilla/components/browser/menu/view/DynamicWidthRecyclerView.kt#L18)

[RecyclerView](#) with automatically set width between widthMin / widthMax xml attributes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DynamicWidthRecyclerView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null)`<br>[RecyclerView](#) with automatically set width between widthMin / widthMax xml attributes. |

### Properties

| Name | Summary |
|---|---|
| [maxWidth](max-width.md) | `var maxWidth: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [minWidth](min-width.md) | `var minWidth: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |

### Functions

| Name | Summary |
|---|---|
| [onMeasure](on-measure.md) | `fun onMeasure(widthSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
