[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [URLStringUtils](./index.md)

# URLStringUtils

`object URLStringUtils` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/URLStringUtils.kt#L12)

### Functions

| Name | Summary |
|---|---|
| [isSearchTerm](is-search-term.md) | `fun isSearchTerm(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Determine whether a string is a search term. |
| [isURLLike](is-u-r-l-like.md) | `fun isURLLike(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Determine whether a string is a URL. |
| [toDisplayUrl](to-display-url.md) | `fun toDisplayUrl(originalUrl: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`): `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)<br>Generates a shorter version of the provided URL for display purposes by stripping it of https/http and/or WWW prefixes when applicable. |
| [toNormalizedURL](to-normalized-u-r-l.md) | `fun toNormalizedURL(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Normalizes a URL String. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
