[android-components](../../index.md) / [mozilla.components.service.location](../index.md) / [MozillaLocationService](index.md) / [hasRegionCached](./has-region-cached.md)

# hasRegionCached

`fun hasRegionCached(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/MozillaLocationService.kt#L81)

Overrides [LocationService.hasRegionCached](../-location-service/has-region-cached.md)

Get if there is already a cached region.
This does not guarantee we have the current actual region but only the last value
which may be obsolete at this time.

