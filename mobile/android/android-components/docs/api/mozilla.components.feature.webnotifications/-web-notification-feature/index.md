[android-components](../../index.md) / [mozilla.components.feature.webnotifications](../index.md) / [WebNotificationFeature](./index.md)

# WebNotificationFeature

`class WebNotificationFeature : `[`WebNotificationDelegate`](../../mozilla.components.concept.engine.webnotifications/-web-notification-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/webnotifications/src/main/java/mozilla/components/feature/webnotifications/WebNotificationFeature.kt#L52)

Feature implementation for configuring and displaying web notifications to the user.

Initialize this feature globally once on app start

``` Kotlin
WebNotificationFeature(
    applicationContext, engine, icons, R.mipmap.ic_launcher, sitePermissionsStorage, BrowserActivity::class.java
)
```

### Parameters

`context` - The application Context.

`engine` - The browser engine.

`browserIcons` - The entry point for loading the large icon for the notification.

`smallIcon` - The small icon for the notification.

`sitePermissionsStorage` - The storage for checking notification site permissions.

`activityClass` - The Activity that the notification will launch if user taps on it

`coroutineContext` - An instance of [CoroutineContext](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html) used for executing async site permission checks.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebNotificationFeature(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, browserIcons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`, smallIcon: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, sitePermissionsStorage: `[`SitePermissionsStorage`](../../mozilla.components.feature.sitepermissions/-site-permissions-storage/index.md)`, activityClass: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<out <ERROR CLASS>>?, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>Feature implementation for configuring and displaying web notifications to the user. |

### Functions

| Name | Summary |
|---|---|
| [onCloseNotification](on-close-notification.md) | `fun onCloseNotification(webNotification: `[`WebNotification`](../../mozilla.components.concept.engine.webnotifications/-web-notification/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web notification is to be closed. |
| [onShowNotification](on-show-notification.md) | `fun onShowNotification(webNotification: `[`WebNotification`](../../mozilla.components.concept.engine.webnotifications/-web-notification/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web notification is to be shown. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
