[android-components](../../index.md) / [mozilla.components.feature.findinpage.view](../index.md) / [FindInPageBar](./index.md)

# FindInPageBar

`class FindInPageBar : ConstraintLayout, `[`FindInPageView`](../-find-in-page-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/findinpage/src/main/java/mozilla/components/feature/findinpage/view/FindInPageBar.kt#L29)

A customizable "Find in page" bar implementing [FindInPageView](../-find-in-page-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FindInPageBar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable "Find in page" bar implementing [FindInPageView](../-find-in-page-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `var listener: `[`Listener`](../-find-in-page-view/-listener/index.md)`?`<br>Listener to be invoked after the user performs certain actions (e.g. "find next result"). |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the UI state. |
| [displayResult](display-result.md) | `fun displayResult(result: `[`FindResultState`](../../mozilla.components.browser.state.state.content/-find-result-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given [FindResultState](../../mozilla.components.browser.state.state.content/-find-result-state/index.md) state in the view. |
| [focus](focus.md) | `fun focus(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests focus for the input element the user can type their query into. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../-find-in-page-view/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [FindInPageView](../-find-in-page-view/index.md) interface to an actual Android [View](#) object. |
