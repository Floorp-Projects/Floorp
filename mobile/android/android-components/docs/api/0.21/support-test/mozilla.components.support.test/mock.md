---
title: mock - 
---

[mozilla.components.support.test](index.html) / [mock](./mock.html)

# mock

`inline fun <reified T : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> mock(): `[`T`](mock.html#T)

Dynamically create a mock object. This method is helpful when creating mocks of classes
using generics.

Instead of:
val foo = Mockito.mock(....Class of Bar?...)

You can just use:
val foo: Bar = mock()

