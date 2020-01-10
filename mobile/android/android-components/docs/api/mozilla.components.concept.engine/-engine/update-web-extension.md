[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [updateWebExtension](./update-web-extension.md)

# updateWebExtension

`open fun updateWebExtension(extension: `[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`, onSuccess: (`[`WebExtension`](../../mozilla.components.concept.engine.webextension/-web-extension/index.md)`?) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L155)

Updates the provided [extension](update-web-extension.md#mozilla.components.concept.engine.Engine$updateWebExtension(mozilla.components.concept.engine.webextension.WebExtension, kotlin.Function1((mozilla.components.concept.engine.webextension.WebExtension, kotlin.Unit)), kotlin.Function2((kotlin.String, kotlin.Throwable, kotlin.Unit)))/extension) if a new version is available.

### Parameters

`extension` - the extension to be updated.

`onSuccess` - (optional) callback invoked if the extension was updated successfully,
providing access to the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) object for bi-directional messaging, if null is provided
that means that the [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md) hasn't been change since the last update.

`onError` - (optional) callback invoked if there was an error updating the extension.
This callback is invoked with an [UnsupportedOperationException](https://developer.android.com/reference/java/lang/UnsupportedOperationException.html) in case the engine doesn't
have web extension support.