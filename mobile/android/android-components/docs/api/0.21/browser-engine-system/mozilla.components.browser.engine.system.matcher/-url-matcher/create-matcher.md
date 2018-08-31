---
title: UrlMatcher.createMatcher - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [UrlMatcher](index.html) / [createMatcher](./create-matcher.html)

# createMatcher

`fun createMatcher(context: Context, blackListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, overrides: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`?, whiteListFile: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`UrlMatcher`](index.html)

Creates a new matcher instance for the provided URL lists.

### Parameters

`context` - the application context

`blackListFile` - resource ID to a JSON file containing the black list

`overrides` - array of resource ID to JSON files containing black list overrides

`whiteListFile` - resource ID to a JSON file containing the white list`fun createMatcher(context: Context, black: `[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`, overrides: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`>?, white: `[`Reader`](http://docs.oracle.com/javase/6/docs/api/java/io/Reader.html)`): `[`UrlMatcher`](index.html)

Creates a new matcher instance for the provided URL lists.

### Parameters

`context` - the application context

`black` - reader containing the black list

`overrides` - array of resource ID to JSON files containing black list overrides

`white` - resource ID to a JSON file containing the white list