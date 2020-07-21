[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [FullScreenFeature](./index.md)

# FullScreenFeature

`open class FullScreenFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/FullScreenFeature.kt#L22)

Feature implementation for handling fullscreen mode (exiting and back button presses).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FullScreenFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, sessionUseCases: `[`SessionUseCases`](../-session-use-cases/index.md)`, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, viewportFitChanged: (`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}, fullScreenChanged: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `open fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>To be called when the back button is pressed, so that only fullscreen mode closes. |
| [start](start.md) | `open fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the feature and a observer to listen for fullscreen changes. |
| [stop](stop.md) | `open fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
