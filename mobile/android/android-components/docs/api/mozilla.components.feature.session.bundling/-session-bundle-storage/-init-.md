[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SessionBundleStorage(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, bundleLifetime: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>)`

A [Session](../../mozilla.components.browser.session/-session/index.md) storage implementation that saves snapshots as a [SessionBundle](../-session-bundle/index.md).

### Parameters

`bundleLifetime` - The lifetime of a bundle controls whether a bundle will be restored or whether this bundle is
considered expired and a new bundle will be used.