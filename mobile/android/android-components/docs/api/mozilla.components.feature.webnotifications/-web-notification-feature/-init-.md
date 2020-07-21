[android-components](../../index.md) / [mozilla.components.feature.webnotifications](../index.md) / [WebNotificationFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebNotificationFeature(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, browserIcons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`, @DrawableRes smallIcon: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, sitePermissionsStorage: `[`SitePermissionsStorage`](../../mozilla.components.feature.sitepermissions/-site-permissions-storage/index.md)`, activityClass: `[`Class`](http://docs.oracle.com/javase/7/docs/api/java/lang/Class.html)`<out <ERROR CLASS>>?, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`

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