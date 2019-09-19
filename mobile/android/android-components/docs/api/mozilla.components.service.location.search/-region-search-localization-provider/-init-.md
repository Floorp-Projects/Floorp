[android-components](../../index.md) / [mozilla.components.service.location.search](../index.md) / [RegionSearchLocalizationProvider](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`RegionSearchLocalizationProvider(service: `[`MozillaLocationService`](../../mozilla.components.service.location/-mozilla-location-service/index.md)`)`

[SearchLocalizationProvider](../../mozilla.components.browser.search.provider.localization/-search-localization-provider/index.md) implementation that uses a [MozillaLocationService](../../mozilla.components.service.location/-mozilla-location-service/index.md) instance to
do a region lookup via GeoIP.

Since this provider may do a network request to determine the region, it is important to only use
the "async" suspension methods of `SearchEngineManager` from the main thread when using this
provider.

