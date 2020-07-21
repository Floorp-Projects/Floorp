[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [UrlMatcher](index.md) / [createMatcher](./create-matcher.md)

# createMatcher

`fun createMatcher(context: <ERROR CLASS>, @RawRes blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @RawRes whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L176)

Creates a new matcher instance for the provided URL lists.

### Parameters

`blackListFile` - resource ID to a JSON file containing the black list

`whiteListFile` - resource ID to a JSON file containing the white list

**Deprecated**
Pass resources directly

`fun createMatcher(resources: <ERROR CLASS>, @RawRes blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, @RawRes whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L190)

Creates a new matcher instance for the provided URL lists.

### Parameters

`blackListFile` - resource ID to a JSON file containing the black list

`whiteListFile` - resource ID to a JSON file containing the white list`fun createMatcher(black: `[`Reader`](http://docs.oracle.com/javase/7/docs/api/java/io/Reader.html)`, white: `[`Reader`](http://docs.oracle.com/javase/7/docs/api/java/io/Reader.html)`, enabledCategories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`> = supportedCategories): `[`UrlMatcher`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt#L207)

Creates a new matcher instance for the provided URL lists.

### Parameters

`black` - reader containing the black list

`white` - resource ID to a JSON file containing the white list