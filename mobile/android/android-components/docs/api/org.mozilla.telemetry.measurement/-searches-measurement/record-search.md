[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [SearchesMeasurement](index.md) / [recordSearch](./record-search.md)

# recordSearch

`open fun recordSearch(@NonNull location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SearchesMeasurement.java#L78)

Record a search for the given location and search engine identifier.

### Parameters

`location` - where search was started.

`identifier` - of the used search engine.