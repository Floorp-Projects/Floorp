[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [WebURLFinder](index.md) / [bestWebURL](./best-web-u-r-l.md)

# bestWebURL

`fun bestWebURL(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/WebURLFinder.kt#L56)

Return best Web URL.

"Best" means a Web URL with a scheme, and failing that, a Web URL without a
scheme.

**Return**
a Web URL or `null`.

