[android-components](../../index.md) / [mozilla.components.service.location](../index.md) / [MozillaLocationService](./index.md)

# MozillaLocationService

`class MozillaLocationService : `[`LocationService`](../-location-service/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/MozillaLocationService.kt#L50)

The Mozilla Location Service (MLS) is an open service which lets devices determine their location
based on network infrastructure like Bluetooth beacons, cell towers and WiFi access points.

* https://location.services.mozilla.com/
* https://mozilla.github.io/ichnaea/api/index.html

Note: Accessing the Mozilla Location Service requires an API token:
https://location.services.mozilla.com/contact

### Parameters

`client` - The HTTP client that this [MozillaLocationService](./index.md) should use for requests.

`apiKey` - The API key that is used to access the Mozilla location service.

`serviceUrl` - An optional URL override usually only needed for testing.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MozillaLocationService(context: <ERROR CLASS>, client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, apiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serviceUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = GEOIP_SERVICE_URL)`<br>The Mozilla Location Service (MLS) is an open service which lets devices determine their location based on network infrastructure like Bluetooth beacons, cell towers and WiFi access points. |

### Functions

| Name | Summary |
|---|---|
| [fetchRegion](fetch-region.md) | `suspend fun fetchRegion(readFromCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Region`](../-location-service/-region/index.md)`?`<br>Determines the current [LocationService.Region](../-location-service/-region/index.md) based on the IP address used to access the service. |
| [hasRegionCached](has-region-cached.md) | `fun hasRegionCached(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Get if there is already a cached region. This does not guarantee we have the current actual region but only the last value which may be obsolete at this time. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
