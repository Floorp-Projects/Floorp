[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [URLStringUtils](index.md) / [isURLLike](./is-u-r-l-like.md)

# isURLLike

`fun isURLLike(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/URLStringUtils.kt#L20)

Determine whether a string is a URL.

This method performs a lenient check to determine whether a string is a URL. Anything that
contains a :, ://, or . and has no internal spaces is potentially a URL. If you need a
stricter check, consider using isURLLikeStrict().

