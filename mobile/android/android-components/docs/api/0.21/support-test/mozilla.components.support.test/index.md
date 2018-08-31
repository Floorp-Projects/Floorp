---
title: mozilla.components.support.test - 
---

[mozilla.components.support.test](./index.html)

## Package mozilla.components.support.test

### Functions

| [any](any.html) | `fun <T> any(): `[`T`](any.html#T)<br>Mockito matcher that matches anything, including nulls and varargs. |
| [eq](eq.html) | `fun <T> eq(value: `[`T`](eq.html#T)`): `[`T`](eq.html#T)<br>Mockito matcher that matches if the argument is the same as the provided value. |
| [expectException](expect-exception.html) | `fun <T : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`> expectException(clazz: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](expect-exception.html#T)`>, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Expect [block](expect-exception.html#mozilla.components.support.test$expectException(kotlin.reflect.KClass((mozilla.components.support.test.expectException.T)), kotlin.Function0((kotlin.Unit)))/block) to throw an exception. Otherwise fail the test (junit). |
| [mock](mock.html) | `fun <T : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> mock(): `[`T`](mock.html#T)<br>Dynamically create a mock object. This method is helpful when creating mocks of classes using generics. |

