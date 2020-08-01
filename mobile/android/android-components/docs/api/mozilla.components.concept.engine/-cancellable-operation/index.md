[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [CancellableOperation](./index.md)

# CancellableOperation

`interface CancellableOperation` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/CancellableOperation.kt#L13)

Represents an async operation that can be cancelled.

### Types

| Name | Summary |
|---|---|
| [Noop](-noop/index.md) | `class Noop : `[`CancellableOperation`](./index.md)<br>Implementation of [CancellableOperation](./index.md) that does nothing (for testing purposes or implementing default methods.) |

### Functions

| Name | Summary |
|---|---|
| [cancel](cancel.md) | `abstract fun cancel(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Cancels this operation. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Noop](-noop/index.md) | `class Noop : `[`CancellableOperation`](./index.md)<br>Implementation of [CancellableOperation](./index.md) that does nothing (for testing purposes or implementing default methods.) |
