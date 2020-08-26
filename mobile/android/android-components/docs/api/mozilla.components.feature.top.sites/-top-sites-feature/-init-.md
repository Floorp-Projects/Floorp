[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSitesFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TopSitesFeature(view: `[`TopSitesView`](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md)`, storage: `[`TopSitesStorage`](../-top-sites-storage/index.md)`, config: () -> `[`TopSitesConfig`](../-top-sites-config/index.md)`, presenter: `[`TopSitesPresenter`](../../mozilla.components.feature.top.sites.presenter/-top-sites-presenter/index.md)` = DefaultTopSitesPresenter(
        view,
        storage,
        config
    ))`

View-bound feature that updates the UI when the [TopSitesStorage](../-top-sites-storage/index.md) is updated.

### Parameters

`view` - An implementor of [TopSitesView](../../mozilla.components.feature.top.sites.view/-top-sites-view/index.md) that will be notified of changes to the storage.

`storage` - The top sites storage that stores pinned and frecent sites.

`config` - Lambda expression that returns [TopSitesConfig](../-top-sites-config/index.md) which species the number of top
sites to return and whether or not to include frequently visited sites.