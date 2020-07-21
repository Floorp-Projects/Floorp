[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtensionRuntime](index.md) / [setAllowedInPrivateBrowsing](./set-allowed-in-private-browsing.md)

# setAllowedInPrivateBrowsing

`open fun setAllowedInPrivateBrowsing(extension: `[`WebExtension`](../-web-extension/index.md)`, allowed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onSuccess: (`[`WebExtension`](../-web-extension/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtensionRuntime.kt#L144)

Sets whether the provided [WebExtension](../-web-extension/index.md) should be allowed to run in private browsing or not.

### Parameters

`extension` - the [WebExtension](../-web-extension/index.md) instance to modify.

`allowed` - true if this extension should be allowed to run in private browsing pages, false otherwise.

`onSuccess` - (optional) callback invoked with modified [WebExtension](../-web-extension/index.md) instance.

`onError` - (optional) callback invoked if there was an error setting private browsing preference
the installed extensions. This callback is invoked with an [UnsupportedOperationException](http://docs.oracle.com/javase/7/docs/api/java/lang/UnsupportedOperationException.html)
in case the engine doesn't have web extension support.