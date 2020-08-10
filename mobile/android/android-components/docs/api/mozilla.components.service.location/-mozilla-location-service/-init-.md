[android-components](../../index.md) / [mozilla.components.service.location](../index.md) / [MozillaLocationService](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`MozillaLocationService(context: <ERROR CLASS>, client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, apiKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, serviceUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = GEOIP_SERVICE_URL)`

The Mozilla Location Service (MLS) is an open service which lets devices determine their location
based on network infrastructure like Bluetooth beacons, cell towers and WiFi access points.

* https://location.services.mozilla.com/
* https://mozilla.github.io/ichnaea/api/index.html

Note: Accessing the Mozilla Location Service requires an API token:
https://location.services.mozilla.com/contact

### Parameters

`client` - The HTTP client that this [MozillaLocationService](index.md) should use for requests.

`apiKey` - The API key that is used to access the Mozilla location service.

`serviceUrl` - An optional URL override usually only needed for testing.