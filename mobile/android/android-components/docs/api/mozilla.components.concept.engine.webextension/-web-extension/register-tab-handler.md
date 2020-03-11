[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](index.md) / [registerTabHandler](./register-tab-handler.md)

# registerTabHandler

`abstract fun registerTabHandler(tabHandler: `[`TabHandler`](../-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L130)

Registers a [TabHandler](../-tab-handler/index.md) for this web extension. This handler will
be invoked whenever a web extension wants to open a new tab. To listen
for session-specific events (such as [TabHandler.onCloseTab](../-tab-handler/on-close-tab.md)) use
registerTabHandler(EngineSession, TabHandler) instead.

### Parameters

`tabHandler` - the [TabHandler](../-tab-handler/index.md) to be invoked when the web extension
wants to open a new tab.`abstract fun registerTabHandler(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, tabHandler: `[`TabHandler`](../-tab-handler/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L139)

Registers a [TabHandler](../-tab-handler/index.md) for the provided [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md). The handler
will be invoked whenever an existing tab should be closed or updated.

### Parameters

`tabHandler` - the [TabHandler](../-tab-handler/index.md) to be invoked when the web extension
wants to update or close an existing tab.