---
title: ObserverRegistry.wrapConsumers - 
---

[mozilla.components.support.base.observer](../index.html) / [ObserverRegistry](index.html) / [wrapConsumers](./wrap-consumers.html)

# wrapConsumers

`fun <V> wrapConsumers(block: `[`T`](index.html#T)`.(`[`V`](wrap-consumers.html#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`V`](wrap-consumers.html#V)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`

Overrides [Observable.wrapConsumers](../-observable/wrap-consumers.html)

Returns a list of lambdas wrapping a consuming method of an observer.

