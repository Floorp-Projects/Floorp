[android-components](../../index.md) / [mozilla.components.support.ktx.kotlin](../index.md) / [kotlin.String](index.md) / [isSameOriginAs](./is-same-origin-as.md)

# isSameOriginAs

`fun `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`.isSameOriginAs(other: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlin/String.kt#L123)

Compares 2 URLs and returns true if they have the same origin,
which means: same protocol, same host, same port.
It will return false if either this or [other](is-same-origin-as.md#mozilla.components.support.ktx.kotlin$isSameOriginAs(kotlin.String, kotlin.String)/other) is not a valid URL.

