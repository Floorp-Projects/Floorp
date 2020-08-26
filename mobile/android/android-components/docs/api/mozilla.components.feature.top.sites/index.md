[android-components](../index.md) / [mozilla.components.feature.top.sites](./index.md)

## Package mozilla.components.feature.top.sites

### Types

| Name | Summary |
|---|---|
| [DefaultTopSitesStorage](-default-top-sites-storage/index.md) | `class DefaultTopSitesStorage : `[`TopSitesStorage`](-top-sites-storage/index.md)`, `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-top-sites-storage/-observer/index.md)`>`<br>Default implementation of [TopSitesStorage](-top-sites-storage/index.md). |
| [PinnedSiteStorage](-pinned-site-storage/index.md) | `class PinnedSiteStorage`<br>A storage implementation for organizing pinned sites. |
| [TopSite](-top-site/index.md) | `data class TopSite`<br>A top site. |
| [TopSitesConfig](-top-sites-config/index.md) | `data class TopSitesConfig`<br>Top sites configuration to specify the number of top sites to display and whether or not to include top frecent sites in the top sites feature. |
| [TopSitesFeature](-top-sites-feature/index.md) | `class TopSitesFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>View-bound feature that updates the UI when the [TopSitesStorage](-top-sites-storage/index.md) is updated. |
| [TopSitesStorage](-top-sites-storage/index.md) | `interface TopSitesStorage : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-top-sites-storage/-observer/index.md)`>`<br>Abstraction layer above the [PinnedSiteStorage](-pinned-site-storage/index.md) and [PlacesHistoryStorage](#) storages. |
| [TopSitesUseCases](-top-sites-use-cases/index.md) | `class TopSitesUseCases`<br>Contains use cases related to the top sites feature. |
