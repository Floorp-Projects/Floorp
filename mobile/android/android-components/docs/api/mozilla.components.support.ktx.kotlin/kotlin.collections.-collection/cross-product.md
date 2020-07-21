[android-components](../../index.md) / [mozilla.components.support.ktx.kotlin](../index.md) / [kotlin.collections.Collection](index.md) / [crossProduct](./cross-product.md)

# crossProduct

`inline fun <T, U, R> `[`Collection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-collection/index.html)`<`[`T`](cross-product.md#T)`>.crossProduct(other: `[`Collection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-collection/index.html)`<`[`U`](cross-product.md#U)`>, block: (`[`T`](cross-product.md#T)`, `[`U`](cross-product.md#U)`) -> `[`R`](cross-product.md#R)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`R`](cross-product.md#R)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlin/Collection.kt#L21)

Performs a cartesian product of all the elements in two collections and returns each pair to
the [block](cross-product.md#mozilla.components.support.ktx.kotlin$crossProduct(kotlin.collections.Collection((mozilla.components.support.ktx.kotlin.crossProduct.T)), kotlin.collections.Collection((mozilla.components.support.ktx.kotlin.crossProduct.U)), kotlin.Function2((mozilla.components.support.ktx.kotlin.crossProduct.T, mozilla.components.support.ktx.kotlin.crossProduct.U, mozilla.components.support.ktx.kotlin.crossProduct.R)))/block) function.

Example:

``` kotlin
val numbers = listOf(1, 2, 3)
val letters = listOf('a', 'b', 'c')
numbers.crossProduct(letters) { number, letter ->
  // Each combination of (1, a), (1, b), (1, c), (2, a), (2, b), etc.
}
```

