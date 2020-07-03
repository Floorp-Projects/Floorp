[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SwipeRefreshFeature](./index.md)

# SwipeRefreshFeature

`class SwipeRefreshFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, OnChildScrollUpCallback, OnRefreshListener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SwipeRefreshFeature.kt#L27)

Feature implementation to add pull to refresh functionality to browsers.

### Parameters

`swipeRefreshLayout` - Reference to SwipeRefreshLayout that has an [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) as its child.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SwipeRefreshFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, reloadUrlUseCase: `[`ReloadUrlUseCase`](../-session-use-cases/-reload-url-use-case/index.md)`, swipeRefreshLayout: SwipeRefreshLayout, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Feature implementation to add pull to refresh functionality to browsers. |

### Functions

| Name | Summary |
|---|---|
| [canChildScrollUp](can-child-scroll-up.md) | `fun canChildScrollUp(parent: SwipeRefreshLayout, child: <ERROR CLASS>?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Callback that checks whether it is possible for the child view to scroll up. If the child view cannot scroll up and the scroll event is not handled by the webpage it means we need to trigger the pull down to refresh functionality. |
| [onRefresh](on-refresh.md) | `fun onRefresh(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a swipe gesture triggers a refresh. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start feature: Starts adding pull to refresh behavior for the active session. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
