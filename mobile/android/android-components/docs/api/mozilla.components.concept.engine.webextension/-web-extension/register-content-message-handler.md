[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [registerContentMessageHandler](./register-content-message-handler.md)

# registerContentMessageHandler

`abstract fun registerContentMessageHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, messageHandler: `[`MessageHandler`](../-message-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L51)

Registers a [MessageHandler](../-message-handler/index.md) for message events from content scripts.

### Parameters

`session` - the session to be observed / attach the message handler to.

`name` - the name of the native "application". This can either be the
name of an application, web extension or a specific feature in case
the web extension opens multiple [Port](../-port/index.md)s. There can only be one handler
with this name per extension and session, and the same name has to be
used in JavaScript when calling `browser.runtime.connectNative` or
`browser.runtime.sendNativeMessage`. Note that name must match
/^\w+(\.\w+)*$/).

`messageHandler` - the message handler to be notified of messaging
events e.g. a port was connected or a message received.