[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSiteStorage](index.md) / [addTopSite](./add-top-site.md)

# addTopSite

`fun addTopSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSiteStorage.kt#L31)

Adds a new [TopSite](../-top-site/index.md).

### Parameters

`title` - The title string.

`url` - The URL string.

`isDefault` - Whether or not the top site added should be a default top site. This is
used to identify top sites that are added by the application.