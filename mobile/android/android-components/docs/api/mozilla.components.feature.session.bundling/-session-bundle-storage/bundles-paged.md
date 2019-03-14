[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [bundlesPaged](./bundles-paged.md)

# bundlesPaged

`fun bundlesPaged(since: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`SessionBundle`](../-session-bundle/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L175)

Returns all saved [SessionBundle](../-session-bundle/index.md) instances as a [DataSource.Factory](#).

A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
`Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.

* https://developer.android.com/topic/libraries/architecture/paging/data
* https://developer.android.com/topic/libraries/architecture/paging/ui

### Parameters

`since` - (Optional) Unix timestamp (milliseconds). If set this method will only return [SessionBundle](../-session-bundle/index.md)
instances that have been saved since the given timestamp.