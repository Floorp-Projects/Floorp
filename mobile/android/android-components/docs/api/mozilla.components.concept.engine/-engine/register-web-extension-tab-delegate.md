[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [registerWebExtensionTabDelegate](./register-web-extension-tab-delegate.md)

# registerWebExtensionTabDelegate

`open fun registerWebExtensionTabDelegate(webExtensionTabDelegate: `[`WebExtensionTabDelegate`](../../mozilla.components.concept.engine.webextension/-web-extension-tab-delegate/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L138)

Registers a [WebExtensionTabDelegate](../../mozilla.components.concept.engine.webextension/-web-extension-tab-delegate/index.md) to be notified when web extensions attempt to open/close tabs.

### Parameters

`webExtensionTabDelegate` - callback is invoked when a web extension opens a new tab.