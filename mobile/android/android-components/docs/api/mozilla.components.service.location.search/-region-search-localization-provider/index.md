[android-components](../../index.md) / [mozilla.components.service.location.search](../index.md) / [RegionSearchLocalizationProvider](./index.md)

# RegionSearchLocalizationProvider

`class RegionSearchLocalizationProvider : `[`SearchLocalizationProvider`](../../mozilla.components.browser.search.provider.localization/-search-localization-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/search/RegionSearchLocalizationProvider.kt#L20)

[SearchLocalizationProvider](../../mozilla.components.browser.search.provider.localization/-search-localization-provider/index.md) implementation that uses a [MozillaLocationService](../../mozilla.components.service.location/-mozilla-location-service/index.md) instance to
do a region lookup via GeoIP.

Since this provider may do a network request to determine the region, it is important to only use
the "async" suspension methods of `SearchEngineManager` from the main thread when using this
provider.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RegionSearchLocalizationProvider(service: `[`MozillaLocationService`](../../mozilla.components.service.location/-mozilla-location-service/index.md)`)`<br>[SearchLocalizationProvider](../../mozilla.components.browser.search.provider.localization/-search-localization-provider/index.md) implementation that uses a [MozillaLocationService](../../mozilla.components.service.location/-mozilla-location-service/index.md) instance to do a region lookup via GeoIP. |

### Functions

| Name | Summary |
|---|---|
| [determineRegion](determine-region.md) | `suspend fun determineRegion(): `[`SearchLocalization`](../../mozilla.components.browser.search.provider.localization/-search-localization/index.md) |
