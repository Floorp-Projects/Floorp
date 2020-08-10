[android-components](../../index.md) / [mozilla.components.concept.engine.webnotifications](../index.md) / [WebNotification](./index.md)

# WebNotification

`data class WebNotification` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webnotifications/WebNotification.kt#L21)

A notification sent by the Web Notifications API.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebNotification(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, sourceUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, iconUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, direction: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, lang: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, requireInteraction: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis())`<br>A notification sent by the Web Notifications API. |

### Properties

| Name | Summary |
|---|---|
| [body](body.md) | `val body: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Body of the notification to be displayed in the second row. |
| [direction](direction.md) | `val direction: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Preference for text direction. |
| [iconUrl](icon-url.md) | `val iconUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Large icon url to display in the notification. Corresponds to [android.app.Notification.Builder.setLargeIcon](#). |
| [lang](lang.md) | `val lang: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>language of the notification. |
| [requireInteraction](require-interaction.md) | `val requireInteraction: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Preference flag that indicates the notification should remain. |
| [sourceUrl](source-url.md) | `val sourceUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the page or Service Worker that generated the notification. |
| [tag](tag.md) | `val tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Tag used to identify the notification. |
| [timestamp](timestamp.md) | `val timestamp: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Time when the notification was created. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Title of the notification to be displayed in the first row. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
