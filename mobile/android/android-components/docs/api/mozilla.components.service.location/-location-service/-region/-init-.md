[android-components](../../../index.md) / [mozilla.components.service.location](../../index.md) / [LocationService](../index.md) / [Region](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Region(countryCode: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, countryName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`

A [Region](index.md) returned by the location service.

The [Region](index.md) use region codes and names from the GENC dataset, which is for the most part
compatible with the ISO 3166 standard. While the API endpoint and [Region](index.md) class refers to
country, no claim about the political status of any region is made by this service.

### Parameters

`countryCode` - Country code; ISO 3166.

`countryName` - Name of the country (English); ISO 3166.