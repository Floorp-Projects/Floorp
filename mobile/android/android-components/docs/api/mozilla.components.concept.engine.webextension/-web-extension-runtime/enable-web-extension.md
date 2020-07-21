[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionRuntime](index.md) / [enableWebExtension](./enable-web-extension.md)

# enableWebExtension

`open fun enableWebExtension(extension: `[`WebExtension`](../-web-extension/index.md)`, source: `[`EnableSource`](../-enable-source/index.md)` = EnableSource.USER, onSuccess: (`[`WebExtension`](../-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionRuntime.kt#L99)

Enables the provided [WebExtension](../-web-extension/index.md). If the extension is already enabled the [onSuccess](enable-web-extension.md#mozilla.components.concept.engine.webextension.WebExtensionRuntime$enableWebExtension(mozilla.components.concept.engine.webextension.WebExtension, mozilla.components.concept.engine.webextension.EnableSource, kotlin.Function1((mozilla.components.concept.engine.webextension.WebExtension, kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/onSuccess)
callback will be invoked, but this method has no effect on the extension.

### Parameters

`extension` - the extension to enable.

`source` - [EnableSource](../-enable-source/index.md) to indicate why the extension is enabled.

`onSuccess` - (optional) callback invoked with the enabled [WebExtension](../-web-extension/index.md)

`onError` - (optional) callback invoked if there was an error enabling
the extensions. This callback is invoked with an [UnsupportedOperationException](http://docs.oracle.com/javase/7/docs/api/java/lang/UnsupportedOperationException.html)
in case the engine doesn't have web extension support.