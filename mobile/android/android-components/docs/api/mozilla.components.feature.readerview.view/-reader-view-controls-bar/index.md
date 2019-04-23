[android-components](../../index.md) / [mozilla.components.feature.readerview.view](../index.md) / [ReaderViewControlsBar](./index.md)

# ReaderViewControlsBar

`class ReaderViewControlsBar : ConstraintLayout, `[`ReaderViewControlsView`](../-reader-view-controls-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/view/ReaderViewControlsBar.kt#L27)

A customizable ReaderView control bar implementing [ReaderViewControlsView](../-reader-view-controls-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReaderViewControlsBar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable ReaderView control bar implementing [ReaderViewControlsView](../-reader-view-controls-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `var listener: `[`Listener`](../-reader-view-controls-view/-listener/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [hideControls](hide-controls.md) | `fun hideControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates visibility to [View.GONE](https://developer.android.com/reference/android/view/View.html#GONE) of the UI controls. |
| [onFocusChanged](on-focus-changed.md) | `fun onFocusChanged(gainFocus: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, direction: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, previouslyFocusedRect: `[`Rect`](https://developer.android.com/reference/android/graphics/Rect.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setColorScheme](set-color-scheme.md) | `fun setColorScheme(scheme: `[`ColorScheme`](../../mozilla.components.feature.readerview/-reader-view-feature/-config/-color-scheme/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the color scheme of the current and future ReaderView sessions. |
| [setFont](set-font.md) | `fun setFont(font: `[`FontType`](../../mozilla.components.feature.readerview/-reader-view-feature/-config/-font-type/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the font type of the current and future ReaderView sessions. |
| [setFontSize](set-font-size.md) | `fun setFontSize(size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the font size of the current and future ReaderView sessions. |
| [showControls](show-controls.md) | `fun showControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates visibility to [View.VISIBLE](https://developer.android.com/reference/android/view/View.html#VISIBLE) and requests focus for the UI controls. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../-reader-view-controls-view/as-view.md) | `open fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Casts this [ReaderViewControlsView](../-reader-view-controls-view/index.md) interface to an actual Android [View](https://developer.android.com/reference/android/view/View.html) object. |

### Extension Properties

| Name | Summary |
|---|---|
| [isLTR](../../mozilla.components.support.ktx.android.view/android.view.-view/is-l-t-r.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](../../mozilla.components.support.ktx.android.view/android.view.-view/is-r-t-l.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Extension Functions

| Name | Summary |
|---|---|
| [forEach](../../mozilla.components.support.ktx.android.view/android.view.-view-group/for-each.md) | `fun `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`.forEach(action: (`[`View`](https://developer.android.com/reference/android/view/View.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs the given action on each View in this ViewGroup. |
| [hideKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/hide-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [isGone](../../mozilla.components.support.ktx.android.view/android.view.-view/is-gone.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isGone(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.GONE. |
| [isInvisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-invisible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isInvisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.INVISIBLE. |
| [isVisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-visible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isVisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.VISIBLE. |
| [setPadding](../../mozilla.components.support.ktx.android.view/android.view.-view/set-padding.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.setPadding(padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../../mozilla.components.support.base.android/-padding/index.md) object. |
| [showKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/show-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
