[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarFeature](./index.md)

# ToolbarFeature

`class ToolbarFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarFeature.kt#L24)

Feature implementation for connecting a toolbar implementation with the session module.

### Types

| Name | Summary |
|---|---|
| [RenderStyle](-render-style/index.md) | `sealed class RenderStyle`<br>Controls how the url should be styled |
| [UrlRenderConfiguration](-url-render-configuration/index.md) | `data class UrlRenderConfiguration`<br>Configuration that controls how URLs are rendered. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarFeature(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, loadUrlUseCase: `[`LoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md)`, searchUseCase: `[`SearchUseCase`](../-search-use-case.md)`? = null, customTabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, urlRenderConfiguration: `[`UrlRenderConfiguration`](-url-render-configuration/index.md)`? = null)`<br>Feature implementation for connecting a toolbar implementation with the session module. |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Handler for back pressed events in activities that use this feature. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: App is in the foreground. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop feature: App is in the background. |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
