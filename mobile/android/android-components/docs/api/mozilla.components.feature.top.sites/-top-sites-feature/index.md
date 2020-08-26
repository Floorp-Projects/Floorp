[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSitesFeature](./index.md)

# TopSitesFeature

`class TopSitesFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSitesFeature.kt#L20)

View-bound feature that updates the UI when the [TopSitesStorage](../-top-sites-storage/index.md) is updated.

### Parameters

`view` - An implementor of [TopSitesView](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md) that will be notified of changes to the storage.

`storage` - The top sites storage that stores pinned and frecent sites.

`config` - Lambda expression that returns [TopSitesConfig](../-top-sites-config/index.md) which species the number of top
sites to return and whether or not to include frequently visited sites.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TopSitesFeature(view: `[`TopSitesView`](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md)`, storage: `[`TopSitesStorage`](../-top-sites-storage/index.md)`, config: () -> `[`TopSitesConfig`](../-top-sites-config/index.md)`, presenter: `[`TopSitesPresenter`](../../mozilla.components.feature.top.sites.presenter/-top-sites-presenter/index.md)` = DefaultTopSitesPresenter(
        view,
        storage,
        config
    ))`<br>View-bound feature that updates the UI when the [TopSitesStorage](../-top-sites-storage/index.md) is updated. |

### Properties

| Name | Summary |
|---|---|
| [config](config.md) | `val config: () -> `[`TopSitesConfig`](../-top-sites-config/index.md)<br>Lambda expression that returns [TopSitesConfig](../-top-sites-config/index.md) which species the number of top sites to return and whether or not to include frequently visited sites. |
| [storage](storage.md) | `val storage: `[`TopSitesStorage`](../-top-sites-storage/index.md)<br>The top sites storage that stores pinned and frecent sites. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
