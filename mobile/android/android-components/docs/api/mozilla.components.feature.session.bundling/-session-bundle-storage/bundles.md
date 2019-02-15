[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [bundles](./bundles.md)

# bundles

`fun bundles(limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 20): LiveData<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SessionBundle`](../-session-bundle/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L124)

Returns the last saved [SessionBundle](../-session-bundle/index.md) instances (up to [limit](bundles.md#mozilla.components.feature.session.bundling.SessionBundleStorage$bundles(kotlin.Int)/limit)) as a [LiveData](#) list.

