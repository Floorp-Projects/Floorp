---
title: Telemetry.recordSearch - 
---

[org.mozilla.telemetry](../index.html) / [Telemetry](index.html) / [recordSearch](./record-search.html)

# recordSearch

`open fun recordSearch(@NonNull location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](index.html)

Record a search for the given location and search engine identifier. Common location values used by Fennec and Focus: actionbar: the user types in the url bar and hits enter to use the default search engine listitem: the user selects a search engine from the list of secondary search engines at the bottom of the screen suggestion: the user clicks on a search suggestion or, in the case that suggestions are disabled, the row corresponding with the main engine

### Parameters

`location` - where search was started.

`identifier` - of the used search engine.