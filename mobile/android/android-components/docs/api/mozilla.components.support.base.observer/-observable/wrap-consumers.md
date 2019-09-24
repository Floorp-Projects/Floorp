[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Observable](index.md) / [wrapConsumers](./wrap-consumers.md)

# wrapConsumers

`abstract fun <R> wrapConsumers(block: `[`T`](index.md#T)`.(`[`R`](wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Observable.kt#L93)

Returns a list of lambdas wrapping a consuming method of an observer.

