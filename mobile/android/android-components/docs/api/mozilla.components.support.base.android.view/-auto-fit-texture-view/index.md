[android-components](../../index.md) / [mozilla.components.support.base.android.view](../index.md) / [AutoFitTextureView](./index.md)

# AutoFitTextureView

`open class AutoFitTextureView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/android/view/AutoFitTextureView.kt#L27)

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
