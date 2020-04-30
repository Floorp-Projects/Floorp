[android-components](../../index.md) / [mozilla.components.support.ktx.kotlin](../index.md) / [kotlin.String](index.md) / [isUrlStrict](./is-url-strict.md)

# isUrlStrict

`fun `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`.~~isUrlStrict~~(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlin/String.kt#L45)
**Deprecated:** Consider using the less strict isUrl or creating a new method using:lib-publicsuffixlist instead. This method is being removed for performance issues

Checks if this string is a URL.

This method performs a strict check to determine whether the string is a URL. It takes longer
to execute than isUrl() but checks whether, e.g., the TLD is ICANN-recognized. Consider
using isUrl() unless these guarantees are required.

