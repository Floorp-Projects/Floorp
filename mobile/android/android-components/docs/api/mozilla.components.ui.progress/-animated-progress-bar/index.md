[android-components](../../index.md) / [mozilla.components.ui.progress](../index.md) / [AnimatedProgressBar](./index.md)

# AnimatedProgressBar

`open class ~~AnimatedProgressBar~~ : `[`ProgressBar`](https://developer.android.com/reference/android/widget/ProgressBar.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/progress/src/main/java/mozilla/components/ui/progress/AnimatedProgressBar.kt#L25)
**Deprecated:** The ui-progress component is deprecated. Using this progress bar affects site loading performance.

An animated progress bar following the Photon Design System.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AnimatedProgressBar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>An animated progress bar following the Photon Design System. |

### Functions

| Name | Summary |
|---|---|
| [onDraw](on-draw.md) | `open fun onDraw(canvas: `[`Canvas`](https://developer.android.com/reference/android/graphics/Canvas.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setProgress](set-progress.md) | `open fun setProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the current progress to the specified value. |
| [setVisibility](set-visibility.md) | `open fun setVisibility(value: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set the visibility state of this view. |

### Extension Properties

| Name | Summary |
|---|---|
| [isLTR](../../mozilla.components.support.ktx.android.view/android.view.-view/is-l-t-r.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](../../mozilla.components.support.ktx.android.view/android.view.-view/is-r-t-l.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Extension Functions

| Name | Summary |
|---|---|
| [getRectWithViewLocation](../../mozilla.components.support.ktx.android.view/android.view.-view/get-rect-with-view-location.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.getRectWithViewLocation(): `[`Rect`](https://developer.android.com/reference/android/graphics/Rect.html)<br>Fills the given [Rect](https://developer.android.com/reference/android/graphics/Rect.html) with data about location view in the window. |
| [hideKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/hide-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [isGone](../../mozilla.components.support.ktx.android.view/android.view.-view/is-gone.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isGone(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.GONE. |
| [isInvisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-invisible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isInvisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.INVISIBLE. |
| [isVisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-visible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isVisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.VISIBLE. |
| [setPadding](../../mozilla.components.support.ktx.android.view/android.view.-view/set-padding.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.setPadding(padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../../mozilla.components.support.base.android/-padding/index.md) object. |
| [showKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/show-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
