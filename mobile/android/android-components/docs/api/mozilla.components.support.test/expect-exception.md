[android-components](../index.md) / [mozilla.components.support.test](index.md) / [expectException](./expect-exception.md)

# expectException

`inline fun <reified T : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`> expectException(clazz: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<`[`T`](expect-exception.md#T)`>, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/Expect.kt#L13)

Expect [block](expect-exception.md#mozilla.components.support.test$expectException(kotlin.reflect.KClass((mozilla.components.support.test.expectException.T)), kotlin.Function0((kotlin.Unit)))/block) to throw an exception. Otherwise fail the test (junit).

