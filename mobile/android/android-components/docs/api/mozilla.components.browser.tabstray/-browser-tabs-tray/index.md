[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [BrowserTabsTray](./index.md)

# BrowserTabsTray

`class BrowserTabsTray : RecyclerView, `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/BrowserTabsTray.kt#L22)

A customizable tabs tray for browsers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserTabsTray(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, tabsAdapter: `[`TabsAdapter`](../-tabs-adapter/index.md)` = TabsAdapter())`<br>A customizable tabs tray for browsers. |

### Properties

| Name | Summary |
|---|---|
| [tabsAdapter](tabs-adapter.md) | `val tabsAdapter: `[`TabsAdapter`](../-tabs-adapter/index.md) |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Convenience method to cast this object to an Android View object. |
| [onDetachedFromWindow](on-detached-from-window.md) | `fun onDetachedFromWindow(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

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
