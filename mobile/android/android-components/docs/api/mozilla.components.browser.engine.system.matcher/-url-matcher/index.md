[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [UrlMatcher](./index.md)

# UrlMatcher

`class UrlMatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L21)

Provides functionality to process categorized URL black/white lists and match
URLs against these lists.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UrlMatcher(patterns: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)` |

### Functions

| Name | Summary |
|---|---|
| [matches](matches.md) | `fun matches(resourceURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>`<br>`fun matches(resourceURI: <ERROR CLASS>, pageURI: <ERROR CLASS>): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>`<br>Checks if the given page URI is blacklisted for the given resource URI. Returns true if the site (page URI) is allowed to access the resource URI, otherwise false. |
| [setCategoriesEnabled](set-categories-enabled.md) | `fun setCategoriesEnabled(categories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables the provided categories. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ADVERTISING](-a-d-v-e-r-t-i-s-i-n-g.md) | `const val ADVERTISING: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ANALYTICS](-a-n-a-l-y-t-i-c-s.md) | `const val ANALYTICS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [CONTENT](-c-o-n-t-e-n-t.md) | `const val CONTENT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DEFAULT](-d-e-f-a-u-l-t.md) | `const val DEFAULT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [DISCONNECT](-d-i-s-c-o-n-n-e-c-t.md) | `const val DISCONNECT: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SOCIAL](-s-o-c-i-a-l.md) | `const val SOCIAL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createMatcher](create-matcher.md) | `fun createMatcher(context: <ERROR CLASS>, blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](./index.md)<br>`fun createMatcher(resources: <ERROR CLASS>, blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](./index.md)<br>`fun createMatcher(black: `[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`, overrides: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`>?, white: `[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](./index.md)<br>Creates a new matcher instance for the provided URL lists. |
| [isWebFont](is-web-font.md) | `fun isWebFont(uri: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the given URI points to a Web font. |
