[android-components](../../index.md) / [mozilla.components.service.location](../index.md) / [MozillaLocationService](index.md) / [fetchRegion](./fetch-region.md)

# fetchRegion

`suspend fun fetchRegion(readFromCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Region`](../-location-service/-region/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/MozillaLocationService.kt#L66)

Overrides [LocationService.fetchRegion](../-location-service/fetch-region.md)

Determines the current [LocationService.Region](../-location-service/-region/index.md) based on the IP address used to access the service.

https://mozilla.github.io/ichnaea/api/region.html

### Parameters

`readFromCache` - Whether a previously returned region (from the cache) can be returned
(default) or whether a request to the service should always be made.