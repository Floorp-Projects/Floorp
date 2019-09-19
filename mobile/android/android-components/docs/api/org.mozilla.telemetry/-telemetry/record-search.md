[android-components](../../index.md) / [org.mozilla.telemetry](../index.md) / [Telemetry](index.md) / [recordSearch](./record-search.md)

# recordSearch

`open fun recordSearch(@NonNull location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/Telemetry.java#L209)

Record a search for the given location and search engine identifier. Common location values used by Fennec and Focus: actionbar: the user types in the url bar and hits enter to use the default search engine listitem: the user selects a search engine from the list of secondary search engines at the bottom of the screen suggestion: the user clicks on a search suggestion or, in the case that suggestions are disabled, the row corresponding with the main engine

### Parameters

`location` - where search was started.

`identifier` - of the used search engine.