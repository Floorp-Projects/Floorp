[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [WindowFeature](./index.md)

# WindowFeature

`class WindowFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/WindowFeature.kt#L17)

Feature implementation for handling window requests.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WindowFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>Feature implementation for handling window requests. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the feature and a observer to listen for window requests. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the feature and the window request observer. |
