[android-components](../../index.md) / [mozilla.components.support.utils.ext](../index.md) / [kotlin.Pair](index.md) / [toNullablePair](./to-nullable-pair.md)

# toNullablePair

`fun <T, U> `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`T`](to-nullable-pair.md#T)`?, `[`U`](to-nullable-pair.md#U)`?>.toNullablePair(): `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`T`](to-nullable-pair.md#T)`, `[`U`](to-nullable-pair.md#U)`>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/ext/Pair.kt#L16)

**Returns**

null if either [first](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/first.html) or [second](#) is null. Otherwise returns a [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) of non-null
values.



Example:
(Object, Object).toNullablePair() == Pair&lt;Object, Object&gt;
(null, Object).toNullablePair() == null
(Object, null).toNullablePair() == null

