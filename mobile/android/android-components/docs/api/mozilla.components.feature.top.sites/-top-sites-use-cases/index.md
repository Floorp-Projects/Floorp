[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSitesUseCases](./index.md)

# TopSitesUseCases

`class TopSitesUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSitesUseCases.kt#L10)

Contains use cases related to the top sites feature.

### Types

| Name | Summary |
|---|---|
| [AddPinnedSiteUseCase](-add-pinned-site-use-case/index.md) | `class AddPinnedSiteUseCase`<br>Add a pinned site use case. |
| [RemoveTopSiteUseCase](-remove-top-site-use-case/index.md) | `class RemoveTopSiteUseCase`<br>Remove a top site use case. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TopSitesUseCases(topSitesStorage: `[`TopSitesStorage`](../-top-sites-storage/index.md)`)`<br>Contains use cases related to the top sites feature. |

### Properties

| Name | Summary |
|---|---|
| [addPinnedSites](add-pinned-sites.md) | `val addPinnedSites: `[`AddPinnedSiteUseCase`](-add-pinned-site-use-case/index.md) |
| [removeTopSites](remove-top-sites.md) | `val removeTopSites: `[`RemoveTopSiteUseCase`](-remove-top-site-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
