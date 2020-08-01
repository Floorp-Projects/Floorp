[android-components](../index.md) / [mozilla.components.support.test](./index.md)

## Package mozilla.components.support.test

### Types

| Name | Summary |
|---|---|
| [KArgumentCaptor](-k-argument-captor/index.md) | `class KArgumentCaptor<out T>` |
| [ThrowProperty](-throw-property/index.md) | `class ThrowProperty<T> : `[`ReadWriteProperty`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html)`<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, `[`T`](-throw-property/index.md#T)`>`<br>A [ReadWriteProperty](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.properties/-read-write-property/index.html) implementation for creating stub properties. |

### Functions

| Name | Summary |
|---|---|
| [any](any.md) | `fun <T> any(): `[`T`](any.md#T)<br>Mockito matcher that matches anything, including nulls and varargs. |
| [argumentCaptor](argument-captor.md) | `fun <T : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> argumentCaptor(): `[`KArgumentCaptor`](-k-argument-captor/index.md)`<`[`T`](argument-captor.md#T)`>`<br>Creates a [KArgumentCaptor](-k-argument-captor/index.md) for given type. |
| [capture](capture.md) | `fun <T> capture(value: ArgumentCaptor<`[`T`](capture.md#T)`>): `[`T`](capture.md#T)<br>Mockito matcher that captures the passed argument. |
| [eq](eq.md) | `fun <T> eq(value: `[`T`](eq.md#T)`): `[`T`](eq.md#T)<br>Mockito matcher that matches if the argument is the same as the provided value. |
| [expectException](expect-exception.md) | `fun <T : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`> expectException(clazz: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](expect-exception.md#T)`>, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Expect [block](expect-exception.md#mozilla.components.support.test$expectException(kotlin.reflect.KClass((mozilla.components.support.test.expectException.T)), kotlin.Function0((kotlin.Unit)))/block) to throw an exception. Otherwise fail the test (junit). |
| [mock](mock.md) | `fun <T : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> mock(setup: `[`T`](mock.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`T`](mock.md#T)<br>Dynamically create a mock object. This method is helpful when creating mocks of classes using generics. |
| [mockMotionEvent](mock-motion-event.md) | `fun mockMotionEvent(action: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, downTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis(), eventTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis(), x: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = 0f, y: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = 0f, metaState: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0): <ERROR CLASS>`<br>Creates a custom [MotionEvent](#) for testing. As of SDK 28 [MotionEvent](#)s can't be mocked anymore and need to be created through [MotionEvent.obtain](#). |
| [not](not.md) | `fun <T> not(value: `[`T`](not.md#T)`): `[`T`](not.md#T)<br>Mockito matcher that matches if the argument is not the same as the provided value. |
| [nullable](nullable.md) | `fun <T> nullable(): `[`T`](nullable.md#T)<br>Mockito matcher that matches anything as nullable. |
| [uninitialized](uninitialized.md) | `fun <T> uninitialized(): `[`T`](uninitialized.md#T) |
| [whenever](whenever.md) | `fun <T> whenever(methodCall: `[`T`](whenever.md#T)`): OngoingStubbing<`[`T`](whenever.md#T)`>`<br>Enables stubbing methods. Use it when you want the mock to return particular value when particular method is called. |
