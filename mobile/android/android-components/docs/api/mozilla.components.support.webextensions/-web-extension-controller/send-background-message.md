[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [sendBackgroundMessage](./send-background-message.md)

# sendBackgroundMessage

`fun sendBackgroundMessage(msg: <ERROR CLASS>, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = messagingId): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L135)

Sends a background message to the provided extension.

### Parameters

`msg` - the message to send

`name` - (optional) name of the port, defaults to the provided [messagingId](#).