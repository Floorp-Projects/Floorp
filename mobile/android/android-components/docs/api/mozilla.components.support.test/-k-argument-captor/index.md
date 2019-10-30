[android-components](../../index.md) / [mozilla.components.support.test](../index.md) / [KArgumentCaptor](./index.md)

# KArgumentCaptor

`class KArgumentCaptor<out T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/KArgumentCaptor.kt#L16)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `KArgumentCaptor(captor: ArgumentCaptor<`[`T`](index.md#T)`>, tClass: `[`KClass`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.reflect/-k-class/index.html)`<*>)` |

### Properties

| Name | Summary |
|---|---|
| [allValues](all-values.md) | `val allValues: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`T`](index.md#T)`>` |
| [value](value.md) | `val value: `[`T`](index.md#T)<br>The first captured value of the argument. |

### Functions

| Name | Summary |
|---|---|
| [capture](capture.md) | `fun capture(): `[`T`](index.md#T) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
