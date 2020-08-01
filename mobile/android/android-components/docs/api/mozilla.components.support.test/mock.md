[android-components](../index.md) / [mozilla.components.support.test](index.md) / [mock](./mock.md)

# mock

`inline fun <reified T : `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`> mock(noinline setup: `[`T`](mock.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`T`](mock.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/Mock.kt#L22)

Dynamically create a mock object. This method is helpful when creating mocks of classes
using generics.

Optional @param setup will be called on the mock after init.

Instead of:
val foo = Mockito.mock(....Class of Bar?...)

You can just use:
val foo: Bar = mock()

