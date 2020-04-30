[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [URLStringUtils](index.md) / [isURLLikeStrict](./is-u-r-l-like-strict.md)

# isURLLikeStrict

`fun ~~isURLLikeStrict~~(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, safe: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/URLStringUtils.kt#L25)
**Deprecated:** Consider using the less strict isURLLike or creating a new method using:lib-publicsuffixlist instead. This method is being removed for performance issues

Determine whether a string is a URL.

This method performs a strict check to determine whether a string is a URL. It takes longer
to execute than isURLLike() but checks whether, e.g., the TLD is ICANN-recognized. Consider
using isURLLike() unless these guarantees are required.

