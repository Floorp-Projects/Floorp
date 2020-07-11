[android-components](../../index.md) / [mozilla.components.service.location](../index.md) / [LocationService](./index.md)

# LocationService

`interface LocationService` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/LocationService.kt#L10)

Interface describing a [LocationService](./index.md) that returns a [Region](-region/index.md).

### Types

| Name | Summary |
|---|---|
| [Region](-region/index.md) | `data class Region`<br>A [Region](-region/index.md) returned by the location service. |

### Functions

| Name | Summary |
|---|---|
| [fetchRegion](fetch-region.md) | `abstract suspend fun fetchRegion(readFromCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Region`](-region/index.md)`?`<br>Determines the current [Region](-region/index.md) of the user. |
| [hasRegionCached](has-region-cached.md) | `abstract fun hasRegionCached(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Get if there is already a cached region. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [dummy](dummy.md) | `fun dummy(): `[`LocationService`](./index.md)<br>Creates a dummy [LocationService](./index.md) implementation that always returns `null`. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MozillaLocationService](../-mozilla-location-service/index.md) | `class MozillaLocationService : `[`LocationService`](./index.md)<br>The Mozilla Location Service (MLS) is an open service which lets devices determine their location based on network infrastructure like Bluetooth beacons, cell towers and WiFi access points. |
