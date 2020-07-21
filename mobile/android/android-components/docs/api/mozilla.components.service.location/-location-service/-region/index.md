[android-components](../../../index.md) / [mozilla.components.service.location](../../index.md) / [LocationService](../index.md) / [Region](./index.md)

# Region

`data class Region` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/location/src/main/java/mozilla/components/service/location/LocationService.kt#L31)

A [Region](./index.md) returned by the location service.

The [Region](./index.md) use region codes and names from the GENC dataset, which is for the most part
compatible with the ISO 3166 standard. While the API endpoint and [Region](./index.md) class refers to
country, no claim about the political status of any region is made by this service.

### Parameters

`countryCode` - Country code; ISO 3166.

`countryName` - Name of the country (English); ISO 3166.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Region(countryCode: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, countryName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>A [Region](./index.md) returned by the location service. |

### Properties

| Name | Summary |
|---|---|
| [countryCode](country-code.md) | `val countryCode: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Country code; ISO 3166. |
| [countryName](country-name.md) | `val countryName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the country (English); ISO 3166. |
