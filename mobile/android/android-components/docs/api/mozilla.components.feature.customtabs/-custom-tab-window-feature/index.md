[android-components](../../index.md) / [mozilla.components.feature.customtabs](../index.md) / [CustomTabWindowFeature](./index.md)

# CustomTabWindowFeature

`class CustomTabWindowFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/CustomTabWindowFeature.kt#L20)

Feature implementation for handling window requests by opening custom tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabWindowFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Feature implementation for handling window requests by opening custom tabs. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts the feature and a observer to listen for window requests. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the feature and the window request observer. |
