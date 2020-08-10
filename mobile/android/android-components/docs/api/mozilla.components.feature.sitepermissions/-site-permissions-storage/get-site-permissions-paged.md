[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsStorage](index.md) / [getSitePermissionsPaged](./get-site-permissions-paged.md)

# getSitePermissionsPaged

`fun getSitePermissionsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`SitePermissions`](../-site-permissions/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsStorage.kt#L92)

Returns all saved [SitePermissions](../-site-permissions/index.md) instances as a [DataSource.Factory](#).

A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
`Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.

* https://developer.android.com/topic/libraries/architecture/paging/data
* https://developer.android.com/topic/libraries/architecture/paging/ui
