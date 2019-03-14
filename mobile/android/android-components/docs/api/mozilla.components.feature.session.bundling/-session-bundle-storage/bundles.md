[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [bundles](./bundles.md)

# bundles

`fun bundles(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0, limit: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 20): LiveData<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SessionBundle`](../-session-bundle/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L153)

Returns the last saved [SessionBundle](../-session-bundle/index.md) instances (up to [limit](bundles.md#mozilla.components.feature.session.bundling.SessionBundleStorage$bundles(kotlin.Long, kotlin.Int)/limit)) as a [LiveData](#) list.

### Parameters

`since` - (Optional) Unix timestamp (milliseconds). If set this method will only return [SessionBundle](../-session-bundle/index.md)
instances that have been saved since the given timestamp.

`limit` - (Optional) Maximum number of [SessionBundle](../-session-bundle/index.md) instances that should be returned.