[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [CancellableOperation](../index.md) / [Noop](./index.md)

# Noop

`class Noop : `[`CancellableOperation`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/CancellableOperation.kt#L19)

Implementation of [CancellableOperation](../index.md) that does nothing (for
testing purposes or implementing default methods.)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Noop()`<br>Implementation of [CancellableOperation](../index.md) that does nothing (for testing purposes or implementing default methods.) |

### Functions

| Name | Summary |
|---|---|
| [cancel](cancel.md) | `fun cancel(): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Cancels this operation. |
