[android-components](../../index.md) / [mozilla.components.feature.findinpage.view](../index.md) / [FindInPageView](./index.md)

# FindInPageView

`interface FindInPageView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/findinpage/src/main/java/mozilla/components/feature/findinpage/view/FindInPageView.kt#L13)

An interface for views that can display "find in page" results and related UI controls.

### Types

| Name | Summary |
|---|---|
| [Listener](-listener/index.md) | `interface Listener` |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `abstract var listener: `[`Listener`](-listener/index.md)`?`<br>Listener to be invoked after the user performs certain actions (e.g. "find next result"). |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [FindInPageView](./index.md) interface to an actual Android [View](#) object. |
| [clear](clear.md) | `abstract fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the UI state. |
| [displayResult](display-result.md) | `abstract fun displayResult(result: `[`FindResultState`](../../mozilla.components.browser.state.state.content/-find-result-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given [FindResultState](../../mozilla.components.browser.state.state.content/-find-result-state/index.md) state in the view. |
| [focus](focus.md) | `abstract fun focus(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Requests focus for the input element the user can type their query into. |

### Inheritors

| Name | Summary |
|---|---|
| [FindInPageBar](../-find-in-page-bar/index.md) | `class FindInPageBar : ConstraintLayout, `[`FindInPageView`](./index.md)<br>A customizable "Find in page" bar implementing [FindInPageView](./index.md). |
