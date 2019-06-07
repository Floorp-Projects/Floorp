[android-components](../../index.md) / [mozilla.components.feature.findinpage.view](../index.md) / [FindInPageBar](./index.md)

# FindInPageBar

`class FindInPageBar : ConstraintLayout, `[`FindInPageView`](../-find-in-page-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/findinpage/src/main/java/mozilla/components/feature/findinpage/view/FindInPageBar.kt#L29)

A customizable "Find in page" bar implementing [FindInPageView](../-find-in-page-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FindInPageBar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable "Find in page" bar implementing [FindInPageView](../-find-in-page-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `var listener: `[`Listener`](../-find-in-page-view/-listener/index.md)`?`<br>Listener to be invoked after the user performs certain actions (e.g. "find next result"). |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the UI state. |
| [displayResult](display-result.md) | `fun displayResult(result: `[`FindResult`](../../mozilla.components.browser.session/-session/-find-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given [Session.FindResult](../../mozilla.components.browser.session/-session/-find-result/index.md) state in the view. |
| [focus](focus.md) | `fun focus(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests focus for the input element the user can type their query into. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../-find-in-page-view/as-view.md) | `open fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Casts this [FindInPageView](../-find-in-page-view/index.md) interface to an actual Android [View](https://developer.android.com/reference/android/view/View.html) object. |

### Extension Properties

| Name | Summary |
|---|---|
| [isLTR](../../mozilla.components.support.ktx.android.view/android.view.-view/is-l-t-r.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](../../mozilla.components.support.ktx.android.view/android.view.-view/is-r-t-l.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Extension Functions

| Name | Summary |
|---|---|
| [forEach](../../mozilla.components.support.ktx.android.view/android.view.-view-group/for-each.md) | `fun `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`.forEach(action: (`[`View`](https://developer.android.com/reference/android/view/View.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs the given action on each View in this ViewGroup. |
| [getRectWithViewLocation](../../mozilla.components.support.ktx.android.view/android.view.-view/get-rect-with-view-location.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.getRectWithViewLocation(): `[`Rect`](https://developer.android.com/reference/android/graphics/Rect.html)<br>Fills the given [Rect](https://developer.android.com/reference/android/graphics/Rect.html) with data about location view in the window. |
| [hideKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/hide-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [isGone](../../mozilla.components.support.ktx.android.view/android.view.-view/is-gone.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isGone(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.GONE. |
| [isInvisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-invisible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isInvisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.INVISIBLE. |
| [isVisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-visible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isVisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.VISIBLE. |
| [setPadding](../../mozilla.components.support.ktx.android.view/android.view.-view/set-padding.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.setPadding(padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../../mozilla.components.support.base.android/-padding/index.md) object. |
| [showKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/show-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
