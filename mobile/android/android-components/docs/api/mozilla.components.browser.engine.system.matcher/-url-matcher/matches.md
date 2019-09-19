[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [UrlMatcher](index.md) / [matches](./matches.md)

# matches

`fun matches(resourceURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L97)
`fun matches(resourceURI: <ERROR CLASS>, pageURI: <ERROR CLASS>): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L112)

Checks if the given page URI is blacklisted for the given resource URI.
Returns true if the site (page URI) is allowed to access
the resource URI, otherwise false.

### Parameters

`resourceURI` - URI of a resource to be loaded by the page

`pageURI` - URI of the page

**Return**
a [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) of &lt;Boolean, String?&gt; the first indicates, if the URI matches and the second
indicates the category of the match if available otherwise null.

