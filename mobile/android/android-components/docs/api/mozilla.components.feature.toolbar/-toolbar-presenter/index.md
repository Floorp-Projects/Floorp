[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarPresenter](./index.md)

# ToolbarPresenter

`class ToolbarPresenter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarPresenter.kt#L27)

Presenter implementation for a toolbar implementation in order to update the toolbar whenever
the state of the selected session changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarPresenter(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, urlRenderConfiguration: `[`UrlRenderConfiguration`](../-toolbar-feature/-url-render-configuration/index.md)`? = null)`<br>Presenter implementation for a toolbar implementation in order to update the toolbar whenever the state of the selected session changes. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start presenter: Display data in toolbar. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
