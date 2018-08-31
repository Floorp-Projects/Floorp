---
title: expectException - 
---

[mozilla.components.support.test](index.html) / [expectException](./expect-exception.html)

# expectException

`inline fun <reified T : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`> expectException(clazz: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](expect-exception.html#T)`>, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Expect [block](expect-exception.html#mozilla.components.support.test$expectException(kotlin.reflect.KClass((mozilla.components.support.test.expectException.T)), kotlin.Function0((kotlin.Unit)))/block) to throw an exception. Otherwise fail the test (junit).

