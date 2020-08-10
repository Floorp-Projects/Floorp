[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSiteStorage](./index.md)

# TopSiteStorage

`class TopSiteStorage` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSiteStorage.kt#L18)

A storage implementation for organizing top sites.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TopSiteStorage(context: <ERROR CLASS>)`<br>A storage implementation for organizing top sites. |

### Functions

| Name | Summary |
|---|---|
| [addTopSite](add-top-site.md) | `fun addTopSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [TopSite](../-top-site/index.md). |
| [getTopSites](get-top-sites.md) | `fun getTopSites(): Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>>`<br>Returns a [Flow](#) list of all the [TopSite](../-top-site/index.md) instances. |
| [getTopSitesPaged](get-top-sites-paged.md) | `fun getTopSitesPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`TopSite`](../-top-site/index.md)`>`<br>Returns all [TopSite](../-top-site/index.md)s as a [DataSource.Factory](#). |
| [removeTopSite](remove-top-site.md) | `fun removeTopSite(site: `[`TopSite`](../-top-site/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [TopSite](../-top-site/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
