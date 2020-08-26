[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSitesConfig](./index.md)

# TopSitesConfig

`data class TopSitesConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSitesConfig.kt#L14)

Top sites configuration to specify the number of top sites to display and
whether or not to include top frecent sites in the top sites feature.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TopSitesConfig(totalSites: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, includeFrecent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>Top sites configuration to specify the number of top sites to display and whether or not to include top frecent sites in the top sites feature. |

### Properties

| Name | Summary |
|---|---|
| [includeFrecent](include-frecent.md) | `val includeFrecent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If true, includes frecent top site results. |
| [totalSites](total-sites.md) | `val totalSites: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>A total number of sites that will be displayed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
