[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [UrlMatcher](index.md) / [createMatcher](./create-matcher.md)

# createMatcher

`fun createMatcher(context: <ERROR CLASS>, @RawRes blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, @RawRes whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L170)

Creates a new matcher instance for the provided URL lists.

### Parameters

`blackListFile` - resource ID to a JSON file containing the black list

`overrides` - array of resource ID to JSON files containing black list overrides

`whiteListFile` - resource ID to a JSON file containing the white list

**Deprecated**
Pass resources directly

`fun createMatcher(resources: <ERROR CLASS>, @RawRes blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, @RawRes whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L186)

Creates a new matcher instance for the provided URL lists.

### Parameters

`blackListFile` - resource ID to a JSON file containing the black list

`overrides` - array of resource ID to JSON files containing black list overrides

`whiteListFile` - resource ID to a JSON file containing the white list`fun createMatcher(black: `[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`, overrides: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`>?, white: `[`Reader`](https://developer.android.com/reference/java/io/Reader.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L206)

Creates a new matcher instance for the provided URL lists.

### Parameters

`black` - reader containing the black list

`overrides` - array of resource ID to JSON files containing black list overrides

`white` - resource ID to a JSON file containing the white list