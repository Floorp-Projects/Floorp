[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [uninstallWebExtension](./uninstall-web-extension.md)

# uninstallWebExtension

`open fun uninstallWebExtension(ext: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L172)

Uninstalls the provided extension from this engine.

### Parameters

`ext` - the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) to uninstall.

`onSuccess` - (optional) callback invoked if the extension was uninstalled successfully.

`onError` - (optional) callback invoked if there was an error uninstalling the extension.
This callback is invoked with an [UnsupportedOperationException](https://developer.android.com/reference/java/lang/UnsupportedOperationException.html) in case the engine doesn't
have web extension support.