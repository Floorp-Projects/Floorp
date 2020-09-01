[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [PinnedSiteStorage](index.md) / [addPinnedSite](./add-pinned-site.md)

# addPinnedSite

`suspend fun addPinnedSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/PinnedSiteStorage.kt#L55)

Adds a new pinned site.

### Parameters

`title` - The title string.

`url` - The URL string.

`isDefault` - Whether or not the pinned site added should be a default pinned site. This
is used to identify pinned sites that are added by the application.