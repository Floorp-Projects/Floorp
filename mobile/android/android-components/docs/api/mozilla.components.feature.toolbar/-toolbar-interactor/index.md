[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarInteractor](./index.md)

# ToolbarInteractor

`class ToolbarInteractor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarInteractor.kt#L15)

Connects a toolbar instance to the browser engine via use cases

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarInteractor(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`, loadUrlUseCase: `[`LoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md)`, searchUseCase: `[`SearchUseCase`](../-search-use-case.md)`? = null)`<br>Connects a toolbar instance to the browser engine via use cases |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts this interactor. Makes sure this interactor is listening to relevant UI changes and triggers the corresponding use-cases in response. |
