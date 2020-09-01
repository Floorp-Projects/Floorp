[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [PinnedSiteStorage](./index.md)

# PinnedSiteStorage

`class PinnedSiteStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/PinnedSiteStorage.kt#L18)

A storage implementation for organizing pinned sites.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PinnedSiteStorage(context: <ERROR CLASS>)`<br>A storage implementation for organizing pinned sites. |

### Functions

| Name | Summary |
|---|---|
| [addAllPinnedSites](add-all-pinned-sites.md) | `suspend fun addAllPinnedSites(topSites: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): <ERROR CLASS>`<br>Adds the given list pinned sites. |
| [addPinnedSite](add-pinned-site.md) | `suspend fun addPinnedSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): <ERROR CLASS>`<br>Adds a new pinned site. |
| [getPinnedSites](get-pinned-sites.md) | `suspend fun getPinnedSites(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>`<br>Returns a list of all the pinned sites. |
| [removePinnedSite](remove-pinned-site.md) | `suspend fun removePinnedSite(site: `[`TopSite`](../-top-site/index.md)`): <ERROR CLASS>`<br>Removes the given pinned site. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
