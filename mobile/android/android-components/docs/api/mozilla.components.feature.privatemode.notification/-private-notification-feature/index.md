[android-components](../../index.md) / [mozilla.components.feature.privatemode.notification](../index.md) / [PrivateNotificationFeature](./index.md)

# PrivateNotificationFeature

`class PrivateNotificationFeature<T : `[`AbstractPrivateNotificationService`](../-abstract-private-notification-service/index.md)`> : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/privatemode/src/main/java/mozilla/components/feature/privatemode/notification/PrivateNotificationFeature.kt#L27)

Starts up a [AbstractPrivateNotificationService](../-abstract-private-notification-service/index.md) once a private tab is opened.

### Parameters

`store` - Browser store reference used to observe the number of private tabs.

`notificationServiceClass` - The service sub-class that should be started by this feature.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PrivateNotificationFeature(context: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, notificationServiceClass: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](index.md#T)`>)`<br>Starts up a [AbstractPrivateNotificationService](../-abstract-private-notification-service/index.md) once a private tab is opened. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
