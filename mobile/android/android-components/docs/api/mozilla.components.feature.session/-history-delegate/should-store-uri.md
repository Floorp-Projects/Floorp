[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [HistoryDelegate](index.md) / [shouldStoreUri](./should-store-uri.md)

# shouldStoreUri

`fun shouldStoreUri(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/HistoryDelegate.kt#L46)

Overrides [HistoryTrackingDelegate.shouldStoreUri](../../mozilla.components.concept.engine.history/-history-tracking-delegate/should-store-uri.md)

Filter out unwanted URIs, such as "chrome:", "about:", etc.
Ported from nsAndroidHistory::CanAddURI
See https://dxr.mozilla.org/mozilla-central/source/mobile/android/components/build/nsAndroidHistory.cpp#326

