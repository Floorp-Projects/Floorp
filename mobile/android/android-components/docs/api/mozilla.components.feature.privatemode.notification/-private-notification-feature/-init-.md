[android-components](../../index.md) / [mozilla.components.feature.privatemode.notification](../index.md) / [PrivateNotificationFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PrivateNotificationFeature(context: <ERROR CLASS>, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, notificationServiceClass: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](index.md#T)`>)`

Starts up a [AbstractPrivateNotificationService](../-abstract-private-notification-service/index.md) once a private tab is opened.

### Parameters

`store` - Browser store reference used to observe the number of private tabs.

`notificationServiceClass` - The service sub-class that should be started by this feature.