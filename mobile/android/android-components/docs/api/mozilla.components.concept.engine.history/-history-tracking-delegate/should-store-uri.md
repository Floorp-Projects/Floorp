[android-components](../../index.md) / [mozilla.components.concept.engine.history](../index.md) / [HistoryTrackingDelegate](index.md) / [shouldStoreUri](./should-store-uri.md)

# shouldStoreUri

`abstract fun shouldStoreUri(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/history/HistoryTrackingDelegate.kt#L41)

Allows an engine to check if this URI is going to be accepted by the delegate.
This helps avoid unnecessary coroutine overhead for URIs which won't be accepted.

