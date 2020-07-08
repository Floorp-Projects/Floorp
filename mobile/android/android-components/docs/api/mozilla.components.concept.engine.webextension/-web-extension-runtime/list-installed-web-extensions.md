[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionRuntime](index.md) / [listInstalledWebExtensions](./list-installed-web-extensions.md)

# listInstalledWebExtensions

`open fun listInstalledWebExtensions(onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`WebExtension`](../-web-extension/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionRuntime.kt#L83)

Lists the currently installed web extensions in this engine.

### Parameters

`onSuccess` - callback invoked with the list of of installed [WebExtension](../-web-extension/index.md)s.

`onError` - (optional) callback invoked if there was an error querying
the installed extensions. This callback is invoked with an [UnsupportedOperationException](http://docs.oracle.com/javase/7/docs/api/java/lang/UnsupportedOperationException.html)
in case the engine doesn't have web extension support.