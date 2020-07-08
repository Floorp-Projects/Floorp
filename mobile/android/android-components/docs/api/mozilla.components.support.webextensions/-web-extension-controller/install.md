[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [install](./install.md)

# install

`fun install(runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L45)

Makes sure the web extension is installed in the provided runtime. If a
content message handler was registered (see
[registerContentMessageHandler](register-content-message-handler.md)) before install completed, registration
will happen upon successful installation.

### Parameters

`runtime` - the [WebExtensionRuntime](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md) the web extension should be installed in.

`onSuccess` - (optional) callback invoked if the extension was installed successfully,
providing access to the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) object.

`onError` - (optional) callback invoked if there was an error installing the extension.