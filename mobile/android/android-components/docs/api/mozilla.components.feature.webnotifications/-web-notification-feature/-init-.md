[android-components](../../index.md) / [mozilla.components.feature.webnotifications](../index.md) / [WebNotificationFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebNotificationFeature(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, browserIcons: `[`BrowserIcons`](../../mozilla.components.browser.icons/-browser-icons/index.md)`, @DrawableRes smallIcon: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, activityClass: `[`Class`](https://developer.android.com/reference/java/lang/Class.html)`<out <ERROR CLASS>>?)`

Feature implementation for configuring and displaying web notifications to the user.

Initialize this feature globally once on app start

``` Kotlin
WebNotificationFeature(
    applicationContext, engine, icons, R.mipmap.ic_launcher, BrowserActivity::class.java
)
```

### Parameters

`context` - The application Context.

`engine` - The browser engine.

`browserIcons` - The entry point for loading the large icon for the notification.

`smallIcon` - The small icon for the notification.

`activityClass` - The Activity that the notification will launch if user taps on it