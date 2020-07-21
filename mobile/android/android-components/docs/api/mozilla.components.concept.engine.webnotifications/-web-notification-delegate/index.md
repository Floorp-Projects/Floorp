[android-components](../../index.md) / [mozilla.components.concept.engine.webnotifications](../index.md) / [WebNotificationDelegate](./index.md)

# WebNotificationDelegate

`interface WebNotificationDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webnotifications/WebNotificationDelegate.kt#L11)

Notifies applications or other components of engine events related to web
notifications e.g. an notification is to be shown or is to be closed

### Functions

| Name | Summary |
|---|---|
| [onCloseNotification](on-close-notification.md) | `open fun onCloseNotification(webNotification: `[`WebNotification`](../-web-notification/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web notification is to be closed. |
| [onShowNotification](on-show-notification.md) | `open fun onShowNotification(webNotification: `[`WebNotification`](../-web-notification/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Invoked when a web notification is to be shown. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [WebNotificationFeature](../../mozilla.components.feature.webnotifications/-web-notification-feature/index.md) | `class WebNotificationFeature : `[`WebNotificationDelegate`](./index.md)<br>Feature implementation for configuring and displaying web notifications to the user. |
