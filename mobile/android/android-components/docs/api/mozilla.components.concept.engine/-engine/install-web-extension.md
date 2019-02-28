[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [installWebExtension](./install-web-extension.md)

# installWebExtension

`open fun installWebExtension(ext: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L69)

Installs the provided extension in this engine.

### Parameters

`ext` - the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) to install.

`onSuccess` - (optional) callback invoked if the extension was installed successfully.

`onError` - (optional) callback invoked if there was an error installing the extension.

### Exceptions

`UnsupportedOperationException` - if this engine doesn't support web extensions.