[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [TabCollectionStorage](index.md) / [getCollectionsPaged](./get-collections-paged.md)

# getCollectionsPaged

`fun getCollectionsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`TabCollection`](../-tab-collection/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/TabCollectionStorage.kt#L105)

Returns all [TabCollection](../-tab-collection/index.md)s as a [DataSource.Factory](#).

A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
`Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.

* https://developer.android.com/topic/libraries/architecture/paging/data
* https://developer.android.com/topic/libraries/architecture/paging/ui
