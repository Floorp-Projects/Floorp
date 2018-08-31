---
title: UrlMatcher - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [UrlMatcher](./index.html)

# UrlMatcher

`class UrlMatcher : OnSharedPreferenceChangeListener`

Provides functionality to process categorized URL black/white lists and match
URLs against these lists.

### Constructors

| [&lt;init&gt;](-init-.html) | `UrlMatcher(patterns: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)` |

### Functions

| [matches](matches.html) | `fun matches(resourceURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, pageURI: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>`fun matches(resourceURI: Uri, pageURI: Uri): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the given page URI is blacklisted for the given resource URI. Returns true if the site (page URI) is allowed to access the resource URI, otherwise false. |
| [onSharedPreferenceChanged](on-shared-preference-changed.html) | `fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, prefName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| [createMatcher](create-matcher.html) | `fun createMatcher(context: Context, blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`UrlMatcher`](./index.md)<br>`fun createMatcher(context: Context, black: `[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`, overrides: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`>?, white: `[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`): `[`UrlMatcher`](./index.md)<br>Creates a new matcher instance for the provided URL lists. |

