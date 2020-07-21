[android-components](../../index.md) / [mozilla.components.feature.tabs](../index.md) / [WindowFeature](./index.md)

# WindowFeature

`class WindowFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/WindowFeature.kt#L21)

Feature implementation for handling window requests by opening and closing tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WindowFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, tabsUseCases: `[`TabsUseCases`](../-tabs-use-cases/index.md)`)`<br>Feature implementation for handling window requests by opening and closing tabs. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing the selected session to listen for window requests and opens / closes tabs as needed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops observing the selected session for incoming window requests. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
