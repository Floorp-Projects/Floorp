---
title: UrlMatcher.matches - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [UrlMatcher](index.html) / [matches](./matches.html)

# matches

`fun matches(resourceURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)
`fun matches(resourceURI: Uri, pageURI: Uri): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)

Checks if the given page URI is blacklisted for the given resource URI.
Returns true if the site (page URI) is allowed to access
the resource URI, otherwise false.

### Parameters

`resourceURI` - URI of a resource to be loaded by the page

`pageURI` - URI of the page