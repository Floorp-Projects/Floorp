[android-components](../../index.md) / [mozilla.components.feature.qr.views](../index.md) / [AutoFitTextureView](./index.md)

# AutoFitTextureView

`open class AutoFitTextureView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/qr/src/main/java/mozilla/components/feature/qr/views/AutoFitTextureView.kt#L27)

A [TextureView](#) that can be adjusted to a specified aspect ratio.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoFitTextureView(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyle: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A [TextureView](#) that can be adjusted to a specified aspect ratio. |

### Functions

| Name | Summary |
|---|---|
| [onMeasure](on-measure.md) | `open fun onMeasure(widthMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setAspectRatio](set-aspect-ratio.md) | `fun setAspectRatio(width: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the aspect ratio for this view. The size of the view will be measured based on the ratio calculated from the parameters. Note that the actual sizes of parameters don't matter, that is, calling setAspectRatio(2, 3) and setAspectRatio(4, 6) make the same result. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
