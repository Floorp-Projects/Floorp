[android-components](../../index.md) / [mozilla.components.feature.findinpage](../index.md) / [FindInPageFeature](./index.md)

# FindInPageFeature

`class FindInPageFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/findinpage/src/main/java/mozilla/components/feature/findinpage/FindInPageFeature.kt#L20)

Feature implementation that will keep a [FindInPageView](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md) in sync with a bound [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FindInPageFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, view: `[`FindInPageView`](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md)`, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, onClose: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`<br>Feature implementation that will keep a [FindInPageView](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md) in sync with a bound [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md). |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `fun bind(session: `[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Binds this feature to the given [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md). Until unbound the [FindInPageView](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md) will be updated presenting the current "Find in Page" state. |
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if the back button press was handled and the feature unbound from a session. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [unbind](unbind.md) | `fun unbind(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unbinds the feature from a previously bound [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md). The [FindInPageView](../../mozilla.components.feature.findinpage.view/-find-in-page-view/index.md) will be cleared and not be updated to present the "Find in Page" state anymore. |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
