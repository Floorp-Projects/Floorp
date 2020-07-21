[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SessionFeature](./index.md)

# SessionFeature

`class SessionFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionFeature.kt#L18)

Feature implementation for connecting the engine module with the session module.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SessionFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, goBackUseCase: `[`GoBackUseCase`](../-session-use-cases/-go-back-use-case/index.md)`, engineSessionUseCases: `[`EngineSessionUseCases`](../../mozilla.components.browser.session.usecases/-engine-session-use-cases/index.md)`, engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md)`, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Feature implementation for connecting the engine module with the session module. |

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Handler for back pressed events in activities that use this feature. |
| [release](release.md) | `fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the feature from rendering sessions on the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) (until explicitly started again) and releases an already rendering session from the [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md). |
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
