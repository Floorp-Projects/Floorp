[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [ObserverRegistry](index.md) / [wrapConsumers](./wrap-consumers.md)

# wrapConsumers

`@Synchronized fun <V> wrapConsumers(block: `[`T`](index.md#T)`.(`[`V`](wrap-consumers.md#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`V`](wrap-consumers.md#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/ObserverRegistry.kt#L152)

Overrides [Observable.wrapConsumers](../-observable/wrap-consumers.md)

Returns a list of lambdas wrapping a consuming method of an observer.

