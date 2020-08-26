[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [DefaultTopSitesStorage](index.md) / [addTopSite](./add-top-site.md)

# addTopSite

`fun addTopSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/DefaultTopSitesStorage.kt#L49)

Overrides [TopSitesStorage.addTopSite](../-top-sites-storage/add-top-site.md)

Adds a new top site.

### Parameters

`title` - The title string.

`url` - The URL string.

`isDefault` - Whether or not the pinned site added should be a default pinned site. This
is used to identify pinned sites that are added by the application.