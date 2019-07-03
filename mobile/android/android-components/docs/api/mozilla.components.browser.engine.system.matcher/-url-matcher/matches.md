[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [UrlMatcher](index.md) / [matches](./matches.md)

# matches

`fun matches(resourceURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L95)
`fun matches(resourceURI: <ERROR CLASS>, pageURI: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L108)

Checks if the given page URI is blacklisted for the given resource URI.
Returns true if the site (page URI) is allowed to access
the resource URI, otherwise false.

### Parameters

`resourceURI` - URI of a resource to be loaded by the page

`pageURI` - URI of the page