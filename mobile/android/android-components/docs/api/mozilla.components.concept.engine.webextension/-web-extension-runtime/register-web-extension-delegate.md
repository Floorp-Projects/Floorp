[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionRuntime](index.md) / [registerWebExtensionDelegate](./register-web-extension-delegate.md)

# registerWebExtensionDelegate

`open fun registerWebExtensionDelegate(webExtensionDelegate: `[`WebExtensionDelegate`](../-web-extension-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionRuntime.kt#L130)

Registers a [WebExtensionDelegate](../-web-extension-delegate/index.md) to be notified of engine events
related to web extensions

### Parameters

`webExtensionDelegate` - callback to be invoked for web extension events.